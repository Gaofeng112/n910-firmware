#!/usr/bin/env xsct

set SCRIPT_DIR [file normalize [file dirname [info script]]]

proc find_project_root {start_dir} {
    set dir [file normalize $start_dir]
    while {1} {
        if {[file exists [file join $dir npu_app/Debug/npu_app.elf]] &&
            [file exists [file join $dir npu_app/_ide/bitstream/design_1_wrapper.bit]] &&
            [file exists [file join $dir npu/export/npu/hw/design_1_wrapper.xsa]]} {
            return $dir
        }
        set parent [file dirname $dir]
        if {$parent eq $dir} {
            error "Cannot find Vitis project root from $start_dir"
        }
        set dir $parent
    }
}

set RESULT_DIR "D:/0_Project/N910/firmware/fpga_results"
set STOP_FILE [file join $RESULT_DIR STOP_AUTOTEST]

if {[info exists ::AUTOTEST_CASE_ROOT]} {
    set CASE_ROOT [file normalize $::AUTOTEST_CASE_ROOT]
    set GROUP_SEQ ""
    if {[info exists ::AUTOTEST_GROUP_SEQ]} {
        set GROUP_SEQ $::AUTOTEST_GROUP_SEQ
    }
    set RESULT_TAG ""
    if {[info exists ::AUTOTEST_RESULT_TAG]} {
        set RESULT_TAG $::AUTOTEST_RESULT_TAG
    }
    set PROJECT_ROOT ""
    if {[info exists ::AUTOTEST_PROJECT_ROOT]} {
        set PROJECT_ROOT [file normalize $::AUTOTEST_PROJECT_ROOT]
    }
} else {
    if {$argc < 1 || $argc > 4} {
        error "Usage: xsct run_case_suite.tcl <case_group_root> [group_seq] [result_tag] [project_root]"
    }

    set CASE_ROOT [file normalize [lindex $argv 0]]
    set GROUP_SEQ ""
    if {$argc >= 2} {
        set GROUP_SEQ [lindex $argv 1]
    }
    set RESULT_TAG ""
    if {$argc >= 3} {
        set RESULT_TAG [lindex $argv 2]
    }
    set PROJECT_ROOT ""
    if {$argc >= 4} {
        set PROJECT_ROOT [file normalize [lindex $argv 3]]
    }
}

if {$PROJECT_ROOT eq "" && [info exists ::env(VITIS_PROJECT_ROOT)]} {
    set PROJECT_ROOT [file normalize $::env(VITIS_PROJECT_ROOT)]
}
if {$PROJECT_ROOT eq ""} {
    set PROJECT_ROOT [find_project_root $SCRIPT_DIR]
}

file mkdir $RESULT_DIR

set BIT_FILE [file join $PROJECT_ROOT npu_app/_ide/bitstream/design_1_wrapper.bit]
set XSA_FILE [file join $PROJECT_ROOT npu/export/npu/hw/design_1_wrapper.xsa]
set ELF_FILE [file join $PROJECT_ROOT npu_app/Debug/npu_app.elf]

set ADDR_CMD       0xE0000000
set ADDR_WEIGHT    0xC0000000
set ADDR_DATA      0xF0000000
set ADDR_CONFIG    0xF4000000
set ADDR_CONFIG_OP 0xF4010000
set ADDR_OPFLOW    0xF4100000
set ADDR_GOLDEN    0xF6000000
set ADDR_SIZE_DESC  0xffff0000
set ADDR_STATUS    0xffff1000
set ADDR_RUN_ID    0xffff1004
set ADDR_RUN_ACK   0xffff1008

set STATUS_IDLE    0x00000000
set STATUS_RUNNING 0x52554E20
set STATUS_PASS    0x50415353
set STATUS_FAIL    0x4641494C
set CASE_TIMEOUT_MS 14400000
set CASE_POLL_MS    200

proc abs_path {path} {
    return [file normalize $path]
}

proc file_size_or_zero {path} {
    if {[file exists $path]} {
        return [file size $path]
    }
    return 0
}

proc choose_existing {paths} {
    foreach p $paths {
        if {[file exists $p]} {
            return $p
        }
    }
    return ""
}

proc require_file {label path} {
    if {$path eq "" || ![file exists $path]} {
        error "Missing required $label file: $path"
    }
}

proc require_runtime_files {} {
    require_file bitstream $::BIT_FILE
    require_file xsa $::XSA_FILE
    require_file elf $::ELF_FILE
}

proc write_size_desc {config config_op opflow cmd weight data golden} {
    set base $::ADDR_SIZE_DESC
    mwr [expr {$base + 0x00}] 0x4e393130
    mwr [expr {$base + 0x04}] 0x00000001
    mwr [expr {$base + 0x08}] [file_size_or_zero $config]
    mwr [expr {$base + 0x0c}] [file_size_or_zero $config_op]
    mwr [expr {$base + 0x10}] [file_size_or_zero $opflow]
    mwr [expr {$base + 0x14}] [file_size_or_zero $cmd]
    mwr [expr {$base + 0x18}] [file_size_or_zero $weight]
    mwr [expr {$base + 0x1c}] [file_size_or_zero $data]
    mwr [expr {$base + 0x20}] [file_size_or_zero $golden]
    mwr [expr {$base + 0x24}] 0
}

proc read_case_status {} {
    return [read_u32 $::ADDR_STATUS]
}

proc read_case_run_ack {} {
    return [read_u32 $::ADDR_RUN_ACK]
}

proc read_u32 {addr} {
    if {[catch {mrd -value $addr} out] == 0} {
        set out [string trim $out]
        if {[parse_u32_token $out value]} {
            return $value
        }
    }

    set out [mrd $addr]
    set matches [regexp -all -inline -nocase {0x[0-9a-f]+|\m[0-9]+\M} $out]
    if {[llength $matches] > 0} {
        set token [lindex $matches end]
        if {[parse_u32_token $token value]} {
            return $value
        }
    }
    error "Unable to parse mrd value from: $out"
}

proc parse_u32_token {token value_name} {
    upvar 1 $value_name value
    set token [string trim $token]
    if {[regexp -nocase {^0x[0-9a-f]+$} $token]} {
        scan $token %x value
        return 1
    }
    if {[regexp {^[0-9]+$} $token]} {
        scan $token %d value
        return 1
    }
    return 0
}

proc start_case_run {run_id} {
    mwr $::ADDR_RUN_ID $run_id
    mwr $::ADDR_RUN_ACK 0
    mwr $::ADDR_STATUS $::STATUS_IDLE
}

proc wait_case_status {run_id timeout_ms poll_ms} {
    set deadline [expr {[clock milliseconds] + $timeout_ms}]
    set next_report [expr {[clock milliseconds] + 5000}]
    while {[clock milliseconds] < $deadline} {
        if {[stop_requested]} {
            return "STOPPED"
        }
        set ack [read_case_run_ack]
        set status [read_case_status]
        if {$status == $::STATUS_PASS} {
            return "PASS"
        }
        if {$status == $::STATUS_FAIL} {
            return "FAIL"
        }
        if {[clock milliseconds] >= $next_report} {
            puts [format "\[WAIT] run_id=0x%08X ack=0x%08X status=0x%08X" $run_id $ack $status]
            set next_report [expr {[clock milliseconds] + 5000}]
        }
        after $poll_ms
    }
    return "TIMEOUT"
}

proc init_hw {} {
    connect -url tcp:127.0.0.1:3121
}

proc select_microblaze {} {
    if {[catch {targets -set -nocase -filter {name =~ "*microblaze*#0" && bscan=="USER2"}}] != 0} {
        targets -set -nocase -filter {name =~ "*microblaze*#0"}
    }
}

proc program_device {} {
    targets -set -filter {level==0}
    fpga -file $::BIT_FILE
    after 1000
    select_microblaze
    loadhw -hw $::XSA_FILE -regs
    configparams mdm-detect-bscan-mask 2
    select_microblaze
}

proc prep_elf {} {
    select_microblaze
    rst -system
    after 3000
    select_microblaze
    dow $::ELF_FILE
}

proc case_dirs {root} {
    set dirs [list]
    if {[file exists [file join $root config.bin]] || [file exists [file join $root case_ncf]]} {
        lappend dirs $root
    }
    foreach d [glob -nocomplain -directory $root -types d *] {
        if {[file exists [file join $d config.bin]] || [file exists [file join $d case_ncf]]} {
            lappend dirs $d
        } else {
            foreach sub [case_dirs $d] {
                lappend dirs $sub
            }
        }
    }
    return [lsort $dirs]
}

proc sanitize_name {name} {
    return [string map {"/" "_" "\\" "_" " " "_" ":" "_"} $name]
}

proc infer_group_from_case {case_dir} {
    set case_id [file tail $case_dir]
    set parent_group [file tail [file dirname $case_dir]]

    if {[string match "v012_MULTIPLE" $parent_group]} {
        set group_name $parent_group
    } elseif {[regexp {^[0-9]+_(v[0-9]+_.+)$} $case_id -> parsed_group]} {
        set group_name $parsed_group
    } else {
        set group_name $parent_group
    }

    if {[regexp {v([0-9]+)_} $group_name -> seq]} {
        set group_seq $seq
    } elseif {[regexp {([0-9]+)} $group_name -> seq]} {
        set group_seq $seq
    } else {
        set group_seq 0
    }

    set group_name [sanitize_name $group_name]
    return [list $group_name $group_seq]
}

proc result_csv_for_group {group_name group_seq} {
    set group_dir [file join $::RESULT_DIR $group_name]
    return [file join $group_dir [format "%s_%s.csv" $group_name $group_seq]]
}

proc ensure_result_csv {csv} {
    if {[info exists ::RESULT_CSV_INITIALIZED($csv)]} {
        return
    }
    file mkdir [file dirname $csv]
    set fp [open $csv w]
    puts $fp "case_id,case_dir,result,detail"
    close $fp
    set ::RESULT_CSV_INITIALIZED($csv) 1
}

proc summarize_groups {case_list} {
    array unset groups
    foreach case_dir $case_list {
        lassign [infer_group_from_case $case_dir] group_name group_seq
        set groups($group_name) 1
    }
    return [array size groups]
}

proc stop_requested {} {
    return [file exists $::STOP_FILE]
}

proc load_case_data {case_dir} {
    if {[file isdirectory [file join $case_dir case_ncf]]} {
        set bin_dir [file join $case_dir case_ncf]
    } else {
        set bin_dir $case_dir
    }

    set config [choose_existing [list \
        [file join $bin_dir dram_new_config.bin] \
        [file join $bin_dir dram_config.bin] \
        [file join $bin_dir config.bin] \
    ]]
    set config_op [file join $bin_dir config_opflow.bin]
    set opflow [choose_existing [list \
        [file join $bin_dir dram_new_opflow.bin] \
        [file join $bin_dir dram_opflow.bin] \
        [file join $bin_dir opflow.bin] \
    ]]
    set cmd [choose_existing [list \
        [file join $bin_dir dram_new_cmd.bin] \
        [file join $bin_dir cmd.bin] \
        [file join $bin_dir dram_cmd.bin] \
    ]]
    set weight [choose_existing [list \
        [file join $bin_dir dram_weight.bin] \
        [file join $bin_dir weight.bin] \
    ]]
    set data [choose_existing [list \
        [file join $case_dir golden dram_input.bin] \
        [file join $bin_dir dram_input.bin] \
        [file join $case_dir dram_input.bin] \
        [file join $bin_dir input.bin] \
        [file join $case_dir input.bin] \
    ]]
    set golden [choose_existing [list \
        [file join $case_dir golden dram_Relu2.bin] \
        [file join $case_dir golden dram_golden.bin] \
        [file join $case_dir golden golden.bin] \
        [file join $bin_dir dram_golden.bin] \
        [file join $bin_dir golden.bin] \
    ]]

    require_file config $config
    require_file opflow $opflow
    require_file weight $weight

    write_size_desc $config $config_op $opflow $cmd $weight $data $golden

    dow -data $config $::ADDR_CONFIG
    if {[file exists $config_op]} {
        dow -data $config_op $::ADDR_CONFIG_OP
    }
    dow -data $opflow $::ADDR_OPFLOW
    if {[file_size_or_zero $cmd] > 0} {
        dow -data $cmd $::ADDR_CMD
    }
    dow -data $weight $::ADDR_WEIGHT
    if {[file_size_or_zero $data] > 0} {
        dow -data $data $::ADDR_DATA
    }
    if {[file_size_or_zero $golden] > 0} {
        dow -data $golden $::ADDR_GOLDEN
    }
}

proc append_result {csv case_id result detail} {
    ensure_result_csv $csv
    set fp [open $csv a]
    puts $fp "$case_id,$::CURRENT_CASE_DIR,$result,$detail"
    close $fp
}

proc run_one_case {case_dir index total} {
    set case_id [file tail $case_dir]
    set ::CURRENT_CASE_DIR $case_dir
    lassign [infer_group_from_case $case_dir] group_name group_seq
    set result_csv [result_csv_for_group $group_name $group_seq]
    incr ::RUN_ID
    puts "\[CASE $index/$total]\[$group_name] start $case_id"

    if {[catch {
        program_device
        prep_elf
        load_case_data $case_dir
        start_case_run $::RUN_ID
        con

        set result [wait_case_status $::RUN_ID $::CASE_TIMEOUT_MS $::CASE_POLL_MS]
        set detail [format "status=0x%08X ack=0x%08X run_id=0x%08X" \
            [read_case_status] [read_case_run_ack] $::RUN_ID]
    } err]} {
        set result ERROR
        set detail [string map {"," ";" "\n" " " "\r" " "} $err]
    }

    append_result $result_csv $case_id $result $detail
    catch {stop}
    puts "\[CASE $index/$total]\[$group_name] done $case_id => $result"
}

proc main {} {
    require_runtime_files
    set ::RUN_ID 0
    array unset ::RESULT_CSV_INITIALIZED
    init_hw
    set dirs [case_dirs $::CASE_ROOT]
    if {[llength $dirs] == 0} {
        error "No case directories found under $::CASE_ROOT"
    }

    set group_count [summarize_groups $dirs]
    puts "\[SUITE] case_root=$::CASE_ROOT cases=[llength $dirs] groups=$group_count"
    puts "\[SUITE] result_dir=$::RESULT_DIR"
    puts "\[SUITE] stop_file=$::STOP_FILE"
    set index 0
    set total [llength $dirs]
    foreach case_dir $dirs {
        if {[stop_requested]} {
            puts "\[SUITE] stop requested before next case"
            break
        }
        incr index
        run_one_case $case_dir $index $total
    }
}

main
