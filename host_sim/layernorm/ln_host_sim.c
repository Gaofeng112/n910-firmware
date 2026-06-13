#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define UNUSED_FN __attribute__((unused))
#else
#define UNUSED_FN
#endif

#define OP_CDMA 2U
#define OP_LAYER_NORM 23U
#define MAX_DATA_NODES 128U
#define MAX_RANK 8U

typedef struct {
    uint8_t *data;
    size_t size;
} blob_t;

typedef struct {
    uint32_t index;
    uint32_t raw_addr;
    uint32_t format;
    uint32_t height;
    uint32_t width;
    uint32_t channel;
} data_node_t;

typedef struct {
    uint32_t input_index;
    uint32_t output_index;
    uint32_t param_addr;
} ln_node_t;

typedef struct {
    int32_t axis;
    int32_t stash_type;
    float epsilon;
    uint32_t x_radix_num;
    const uint8_t *x_radix;
    uint32_t y_radix_num;
    const uint8_t *y_radix;
    uint32_t scale_rank;
    uint32_t scale_shape[MAX_RANK];
    uint32_t scale_len;
    const uint8_t *scale_data;
    uint32_t bias_rank;
    uint32_t bias_shape[MAX_RANK];
    uint32_t bias_len;
    const uint8_t *bias_data;
} ln_param_t;

typedef struct {
    uint32_t height;
    uint32_t width;
    uint32_t channel;
    int16_t *data;
} normal_tensor_t;

typedef struct {
    uint32_t height;
    uint32_t width;
    uint32_t channel;
    int16_t *data;
} format1_tensor_t;

static uint32_t read_be_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static int32_t read_be_i32(const uint8_t *p)
{
    return (int32_t)read_be_u32(p);
}

static int16_t read_be_i16(const uint8_t *p)
{
    return (int16_t)(((uint32_t)p[0] << 8) | (uint32_t)p[1]);
}

static int16_t read_le_i16(const uint8_t *p)
{
    return (int16_t)(((uint32_t)p[1] << 8) | (uint32_t)p[0]);
}

static void write_le_i16(uint8_t *p, int16_t val)
{
    p[0] = (uint8_t)((uint16_t)val & 0xffU);
    p[1] = (uint8_t)(((uint16_t)val >> 8) & 0xffU);
}

static float word_to_float(uint32_t word)
{
    union {
        uint32_t word;
        float val;
    } raw;

    raw.word = word;
    return raw.val;
}

static float read_be_f32(const uint8_t *p)
{
    return word_to_float(read_be_u32(p));
}

static float radix_scale(int16_t radix)
{
    float scale = 1.0f;
    int i;

    if (radix > 0) {
        for (i = 0; i < radix; ++i) {
            scale *= 2.0f;
        }
    } else {
        for (i = 0; i < -radix; ++i) {
            scale *= 0.5f;
        }
    }

    return scale;
}

static size_t normal_offset(uint32_t height,
                            uint32_t width,
                            uint32_t channel,
                            uint32_t c,
                            uint32_t h,
                            uint32_t w)
{
    (void)height;
    return (((size_t)h * width) + w) * channel + c;
}

static size_t format1_offset(uint32_t height,
                             uint32_t width,
                             uint32_t c,
                             uint32_t h,
                             uint32_t w)
{
    uint32_t c_group = c >> 5;
    uint32_t c_in_group = c & 0x1fU;
    uint32_t c_quad = (c_in_group >> 2) & 0x3U;
    uint32_t c_hi = c_in_group >> 4;
    uint32_t c_low = c & 0x3U;
    size_t offset = c_group;

    offset = (offset * height) + h;
    offset = (offset * width) + w;
    offset = (offset * 4U) + c_quad;
    offset = (offset * 2U) + c_hi;
    offset = (offset * 4U) + c_low;
    return offset;
}

static size_t format1_element_count(uint32_t height, uint32_t width, uint32_t channel)
{
    uint32_t channel_aligned = (channel + 31U) & ~31U;

    return (size_t)height * width * channel_aligned;
}

static float ln_sqrtf(float val)
{
    uint32_t i;
    float x;

    if (val <= 0.0f) {
        return 0.0f;
    }

    x = (val > 1.0f) ? val : 1.0f;
    for (i = 0U; i < 16U; ++i) {
        x = 0.5f * (x + (val / x));
    }

    return x;
}

static int16_t round_to_i16(float val)
{
    if (val >= 0.0f) {
        return (int16_t)(val + 0.5f);
    }

    return (int16_t)(val - 0.5f);
}

static int16_t param_radix_at(const uint8_t *radix_data, uint32_t radix_num, uint32_t c)
{
    uint32_t idx = (radix_num == 1U) ? 0U : c;
    return read_be_i16(radix_data + ((size_t)idx * sizeof(int16_t)));
}

static blob_t read_file(const char *path)
{
    blob_t blob;
    FILE *fp;

    blob.data = NULL;
    blob.size = 0U;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "open failed: %s\n", path);
        return blob;
    }

    fseek(fp, 0, SEEK_END);
    blob.size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    blob.data = (uint8_t *)malloc(blob.size);
    if (blob.data == NULL) {
        fclose(fp);
        fprintf(stderr, "malloc failed: %s\n", path);
        blob.size = 0U;
        return blob;
    }

    if (fread(blob.data, 1U, blob.size, fp) != blob.size) {
        fprintf(stderr, "read failed: %s\n", path);
        free(blob.data);
        blob.data = NULL;
        blob.size = 0U;
    }
    fclose(fp);
    return blob;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "open write failed: %s\n", path);
        return -1;
    }

    if (fwrite(data, 1U, size, fp) != size) {
        fclose(fp);
        fprintf(stderr, "write failed: %s\n", path);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int load_normal_tensor(const blob_t *input, const data_node_t *node, normal_tensor_t *tensor)
{
    size_t i;
    size_t element_count = (size_t)node->height * node->width * node->channel;

    tensor->height = node->height;
    tensor->width = node->width;
    tensor->channel = node->channel;
    tensor->data = NULL;

    if (input->size != (element_count * sizeof(int16_t))) {
        fprintf(stderr, "input size mismatch: got=%zu expect=%zu\n",
                input->size,
                element_count * sizeof(int16_t));
        return -1;
    }

    tensor->data = (int16_t *)malloc(element_count * sizeof(int16_t));
    if (tensor->data == NULL) {
        fprintf(stderr, "normal tensor malloc failed\n");
        return -2;
    }

    for (i = 0U; i < element_count; ++i) {
        tensor->data[i] = read_le_i16(input->data + (i * sizeof(int16_t)));
    }

    return 0;
}

static void free_normal_tensor(normal_tensor_t *tensor)
{
    free(tensor->data);
    tensor->data = NULL;
}

static int init_format1_tensor(const data_node_t *node, format1_tensor_t *tensor)
{
    size_t element_count = (size_t)node->height * node->width * node->channel;

    tensor->height = node->height;
    tensor->width = node->width;
    tensor->channel = node->channel;
    tensor->data = (int16_t *)calloc(element_count, sizeof(int16_t));
    if (tensor->data == NULL) {
        fprintf(stderr, "format1 tensor malloc failed\n");
        return -1;
    }

    return 0;
}

static void free_format1_tensor(format1_tensor_t *tensor)
{
    free(tensor->data);
    tensor->data = NULL;
}

static int normal_to_format1(const normal_tensor_t *normal, format1_tensor_t *format1)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    if ((normal->height != format1->height) ||
        (normal->width != format1->width) ||
        (normal->channel != format1->channel)) {
        return -1;
    }

    for (h = 0U; h < normal->height; ++h) {
        for (w = 0U; w < normal->width; ++w) {
            for (c = 0U; c < normal->channel; ++c) {
                size_t src = normal_offset(normal->height, normal->width, normal->channel, c, h, w);
                size_t dst = format1_offset(format1->height, format1->width, c, h, w);

                format1->data[dst] = normal->data[src];
            }
        }
    }

    return 0;
}

static int16_t format1_read_i16(const format1_tensor_t *tensor, uint32_t c, uint32_t h, uint32_t w)
{
    return tensor->data[format1_offset(tensor->height, tensor->width, c, h, w)];
}

static void format1_write_i16(format1_tensor_t *tensor, uint32_t c, uint32_t h, uint32_t w, int16_t val)
{
    tensor->data[format1_offset(tensor->height, tensor->width, c, h, w)] = val;
}

static int format1_to_normal_bytes(const format1_tensor_t *format1, uint8_t *normal)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    for (h = 0U; h < format1->height; ++h) {
        for (w = 0U; w < format1->width; ++w) {
            for (c = 0U; c < format1->channel; ++c) {
                size_t dst = normal_offset(format1->height, format1->width, format1->channel, c, h, w);
                int16_t raw = format1_read_i16(format1, c, h, w);

                write_le_i16(normal + (dst * sizeof(int16_t)), raw);
            }
        }
    }

    return 0;
}

static void make_path(char *out, size_t out_size, const char *dir, const char *name)
{
    snprintf(out, out_size, "%s/%s", dir, name);
}

static int find_data_node(const data_node_t *nodes, uint32_t count, uint32_t index, data_node_t *out)
{
    uint32_t i;

    for (i = 0U; i < count; ++i) {
        if (nodes[i].index == index) {
            *out = nodes[i];
            return 0;
        }
    }

    return -1;
}

static int parse_opflow(const blob_t *opflow, ln_node_t *ln, data_node_t *input, data_node_t *output)
{
    size_t off = 0U;
    data_node_t nodes[MAX_DATA_NODES];
    uint32_t node_count = 0U;
    uint32_t found_ln = 0U;

    memset(ln, 0, sizeof(*ln));
    while (off < opflow->size) {
        uint32_t index;
        uint32_t type;

        if ((opflow->size - off) < (2U * sizeof(uint32_t))) {
            return -1;
        }

        index = read_be_u32(opflow->data + off);
        type = read_be_u32(opflow->data + off + 4U);

        if (type == 0U) {
            uint32_t opcode;

            if ((opflow->size - off) < (4U * sizeof(uint32_t))) {
                return -2;
            }
            opcode = read_be_u32(opflow->data + off + 12U);
            if (opcode == OP_CDMA) {
                off += 6U * sizeof(uint32_t);
                continue;
            }

            if (opcode == OP_LAYER_NORM) {
                uint32_t input_num = read_be_u32(opflow->data + off + 16U);
                if (input_num != 1U) {
                    fprintf(stderr, "unsupported LN input num=%u\n", input_num);
                    return -3;
                }
                ln->input_index = read_be_u32(opflow->data + off + 20U);
                ln->output_index = read_be_u32(opflow->data + off + 24U);
                ln->param_addr = read_be_u32(opflow->data + off + 28U);
                found_ln = 1U;
                off += (7U + input_num) * sizeof(uint32_t);
                continue;
            }

            fprintf(stderr, "unsupported CPU opcode=%u index=%u\n", opcode, index);
            return -4;
        }

        if (type == 1U) {
            off += 3U * sizeof(uint32_t);
            continue;
        }

        if (type == 2U) {
            data_node_t node;

            if ((opflow->size - off) < (7U * sizeof(uint32_t))) {
                return -5;
            }
            if (node_count >= MAX_DATA_NODES) {
                return -6;
            }

            node.index = index;
            node.raw_addr = read_be_u32(opflow->data + off + 8U);
            node.format = read_be_u32(opflow->data + off + 12U);
            node.height = read_be_u32(opflow->data + off + 16U);
            node.width = read_be_u32(opflow->data + off + 20U);
            node.channel = read_be_u32(opflow->data + off + 24U);
            nodes[node_count++] = node;
            off += 7U * sizeof(uint32_t);
            continue;
        }

        fprintf(stderr, "unsupported node type=%u index=%u\n", type, index);
        return -7;
    }

    if (found_ln == 0U) {
        return -8;
    }
    if (find_data_node(nodes, node_count, ln->input_index, input) != 0) {
        return -9;
    }
    if (find_data_node(nodes, node_count, ln->output_index, output) != 0) {
        return -10;
    }

    return 0;
}

static int parse_tensor(const blob_t *weight,
                        size_t *off,
                        uint32_t *rank,
                        uint32_t shape[MAX_RANK],
                        uint32_t *len,
                        const uint8_t **data)
{
    uint32_t i;

    if ((*off + sizeof(uint32_t)) > weight->size) {
        return -1;
    }
    *rank = read_be_u32(weight->data + *off);
    *off += sizeof(uint32_t);
    if (*rank > MAX_RANK) {
        return -2;
    }

    for (i = 0U; i < *rank; ++i) {
        if ((*off + sizeof(uint32_t)) > weight->size) {
            return -3;
        }
        shape[i] = read_be_u32(weight->data + *off);
        *off += sizeof(uint32_t);
    }

    if ((*off + sizeof(uint32_t)) > weight->size) {
        return -4;
    }
    *len = read_be_u32(weight->data + *off);
    *off += sizeof(uint32_t);
    if ((*off + ((size_t)*len * sizeof(uint32_t))) > weight->size) {
        return -5;
    }

    *data = weight->data + *off;
    *off += (size_t)*len * sizeof(uint32_t);
    return 0;
}

static int parse_weight(const blob_t *weight, ln_param_t *param)
{
    size_t off = 0U;

    memset(param, 0, sizeof(*param));
    if (weight->size < 12U) {
        return -1;
    }

    param->axis = read_be_i32(weight->data + off);
    off += sizeof(uint32_t);
    param->stash_type = read_be_i32(weight->data + off);
    off += sizeof(uint32_t);
    param->epsilon = read_be_f32(weight->data + off);
    off += sizeof(uint32_t);

    if ((off + sizeof(uint32_t)) > weight->size) {
        return -2;
    }
    param->x_radix_num = read_be_u32(weight->data + off);
    off += sizeof(uint32_t);
    if ((off + ((size_t)param->x_radix_num * sizeof(int16_t))) > weight->size) {
        return -3;
    }
    param->x_radix = weight->data + off;
    off += (size_t)param->x_radix_num * sizeof(int16_t);

    if ((off + sizeof(uint32_t)) > weight->size) {
        return -4;
    }
    param->y_radix_num = read_be_u32(weight->data + off);
    off += sizeof(uint32_t);
    if ((off + ((size_t)param->y_radix_num * sizeof(int16_t))) > weight->size) {
        return -5;
    }
    param->y_radix = weight->data + off;
    off += (size_t)param->y_radix_num * sizeof(int16_t);

    if (parse_tensor(weight,
                     &off,
                     &param->scale_rank,
                     param->scale_shape,
                     &param->scale_len,
                     &param->scale_data) != 0) {
        return -6;
    }
    if (parse_tensor(weight,
                     &off,
                     &param->bias_rank,
                     param->bias_shape,
                     &param->bias_len,
                     &param->bias_data) != 0) {
        return -7;
    }
    if (off != weight->size) {
        return -8;
    }

    return 0;
}

static int UNUSED_FN run_layernorm_normal(const data_node_t *node,
                                          const ln_param_t *param,
                                          const normal_tensor_t *input,
                                          uint8_t *result)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    if ((input->height != node->height) ||
        (input->width != node->width) ||
        (input->channel != node->channel)) {
        fprintf(stderr, "input shape mismatch: input=[%u,%u,%u] node=[%u,%u,%u]\n",
                input->height,
                input->width,
                input->channel,
                node->height,
                node->width,
                node->channel);
        return -1;
    }
    if ((param->scale_len < node->channel) ||
        (param->bias_len < node->channel) ||
        ((param->x_radix_num != 1U) && (param->x_radix_num < node->channel)) ||
        ((param->y_radix_num != 1U) && (param->y_radix_num < node->channel))) {
        fprintf(stderr, "bad LN param len: scale=%u bias=%u x_radix=%u y_radix=%u channel=%u\n",
                param->scale_len,
                param->bias_len,
                param->x_radix_num,
                param->y_radix_num,
                node->channel);
        return -2;
    }

    for (h = 0U; h < node->height; ++h) {
        for (w = 0U; w < node->width; ++w) {
            float sum = 0.0f;
            float mean;
            float var_sum = 0.0f;
            float denom;

            for (c = 0U; c < node->channel; ++c) {
                size_t idx = (((size_t)h * node->width) + w) * node->channel + c;
                int16_t raw = input->data[idx];
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                sum += (float)raw / radix_scale(x_radix);
            }

            mean = sum / (float)node->channel;
            for (c = 0U; c < node->channel; ++c) {
                size_t idx = (((size_t)h * node->width) + w) * node->channel + c;
                int16_t raw = input->data[idx];
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                float val = (float)raw / radix_scale(x_radix);
                float diff = val - mean;
                var_sum += diff * diff;
            }

            denom = ln_sqrtf((var_sum / (float)node->channel) + param->epsilon);
            for (c = 0U; c < node->channel; ++c) {
                size_t idx = (((size_t)h * node->width) + w) * node->channel + c;
                int16_t raw = input->data[idx];
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                int16_t y_radix = param_radix_at(param->y_radix, param->y_radix_num, c);
                float scale = read_be_f32(param->scale_data + ((size_t)c * sizeof(uint32_t)));
                float bias = read_be_f32(param->bias_data + ((size_t)c * sizeof(uint32_t)));
                float val = (float)raw / radix_scale(x_radix);
                float out_val = ((val - mean) / denom) * scale + bias;
                int16_t out_raw = round_to_i16(out_val * radix_scale(y_radix));

                write_le_i16(result + (idx * sizeof(int16_t)), out_raw);
            }
        }
    }

    return 0;
}

static int run_layernorm_format1_input(const data_node_t *node,
                                       const ln_param_t *param,
                                       const format1_tensor_t *input,
                                       format1_tensor_t *output)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    if ((input->height != node->height) ||
        (input->width != node->width) ||
        (input->channel != node->channel) ||
        (output->height != node->height) ||
        (output->width != node->width) ||
        (output->channel != node->channel)) {
        fprintf(stderr, "format1 shape mismatch: input=[%u,%u,%u] output=[%u,%u,%u] node=[%u,%u,%u]\n",
                input->height,
                input->width,
                input->channel,
                output->height,
                output->width,
                output->channel,
                node->height,
                node->width,
                node->channel);
        return -1;
    }
    if ((param->scale_len < node->channel) ||
        (param->bias_len < node->channel) ||
        ((param->x_radix_num != 1U) && (param->x_radix_num < node->channel)) ||
        ((param->y_radix_num != 1U) && (param->y_radix_num < node->channel))) {
        fprintf(stderr, "bad LN param len: scale=%u bias=%u x_radix=%u y_radix=%u channel=%u\n",
                param->scale_len,
                param->bias_len,
                param->x_radix_num,
                param->y_radix_num,
                node->channel);
        return -2;
    }

    for (h = 0U; h < node->height; ++h) {
        for (w = 0U; w < node->width; ++w) {
            float sum = 0.0f;
            float mean;
            float var_sum = 0.0f;
            float denom;

            for (c = 0U; c < node->channel; ++c) {
                int16_t raw = format1_read_i16(input, c, h, w);
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                sum += (float)raw / radix_scale(x_radix);
            }

            mean = sum / (float)node->channel;
            for (c = 0U; c < node->channel; ++c) {
                int16_t raw = format1_read_i16(input, c, h, w);
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                float val = (float)raw / radix_scale(x_radix);
                float diff = val - mean;
                var_sum += diff * diff;
            }

            denom = ln_sqrtf((var_sum / (float)node->channel) + param->epsilon);
            for (c = 0U; c < node->channel; ++c) {
                int16_t raw = format1_read_i16(input, c, h, w);
                int16_t x_radix = param_radix_at(param->x_radix, param->x_radix_num, c);
                int16_t y_radix = param_radix_at(param->y_radix, param->y_radix_num, c);
                float scale = read_be_f32(param->scale_data + ((size_t)c * sizeof(uint32_t)));
                float bias = read_be_f32(param->bias_data + ((size_t)c * sizeof(uint32_t)));
                float val = (float)raw / radix_scale(x_radix);
                float out_val = ((val - mean) / denom) * scale + bias;
                int16_t out_raw = round_to_i16(out_val * radix_scale(y_radix));

                format1_write_i16(output, c, h, w, out_raw);
            }
        }
    }

    return 0;
}

static int compare_result(const uint8_t *result,
                          const blob_t *golden,
                          const data_node_t *node,
                          const char *report_path)
{
    size_t i;
    size_t count = golden->size / sizeof(int16_t);
    uint32_t exact_mismatch = 0U;
    uint32_t diff_gt_1 = 0U;
    uint32_t max_abs_diff = 0U;
    uint32_t diff_neg_1 = 0U;
    uint32_t diff_pos_1 = 0U;
    uint64_t sum_abs_diff = 0U;
    uint64_t sum_sq_diff = 0U;
    FILE *report = fopen(report_path, "w");

    if (report == NULL) {
        fprintf(stderr, "open diff report failed: %s\n", report_path);
        return 2;
    }
    fprintf(report, "idx,h,w,c,out,gold,diff,abs_diff\n");

    for (i = 0U; i < count; ++i) {
        int out = (int)read_le_i16(result + (i * sizeof(int16_t)));
        int gold = (int)read_le_i16(golden->data + (i * sizeof(int16_t)));
        int diff = out - gold;
        uint32_t abs_diff = (uint32_t)((diff < 0) ? -diff : diff);

        if (diff == -1) {
            diff_neg_1++;
        } else if (diff == 1) {
            diff_pos_1++;
        }
        sum_abs_diff += abs_diff;
        sum_sq_diff += (uint64_t)abs_diff * abs_diff;
        if (abs_diff > max_abs_diff) {
            max_abs_diff = abs_diff;
        }
        if (abs_diff != 0U) {
            size_t hw = (size_t)node->width * node->channel;
            size_t h = i / hw;
            size_t rem = i % hw;
            size_t w = rem / node->channel;
            size_t c = rem % node->channel;

            if (exact_mismatch < 8U) {
                printf("[CMP] mismatch idx=%zu out=%d gold=%d diff=%d\n", i, out, gold, diff);
            }
            fprintf(report, "%zu,%zu,%zu,%zu,%d,%d,%d,%u\n",
                    i,
                    h,
                    w,
                    c,
                    out,
                    gold,
                    diff,
                    abs_diff);
            exact_mismatch++;
        }
        if (abs_diff > 1U) {
            diff_gt_1++;
        }
    }
    fclose(report);

    {
        double mae = (double)sum_abs_diff / (double)count;
        double mse = (double)sum_sq_diff / (double)count;
        double rmse = (double)ln_sqrtf((float)mse);
        double exact_rate = ((double)(count - exact_mismatch) * 100.0) / (double)count;
        double tol1_rate = ((double)(count - diff_gt_1) * 100.0) / (double)count;

        printf("[CMP] exact_mismatch=%u diff_neg_1=%u diff_pos_1=%u diff_gt_1=%u max_abs_diff=%u\n",
               exact_mismatch,
               diff_neg_1,
               diff_pos_1,
               diff_gt_1,
               max_abs_diff);
        printf("[CMP] mae=%.8f mse=%.8f rmse=%.8f exact_rate=%.6f%% tol1_rate=%.6f%%\n",
               mae,
               mse,
               rmse,
               exact_rate,
               tol1_rate);
    }
    printf("[CMP] diff_report=%s\n", report_path);

    if (exact_mismatch == 0U) {
        return 0;
    }
    if (diff_gt_1 == 0U) {
        return 1;
    }
    return 2;
}

static int check_format1_roundtrip_sample(const normal_tensor_t *normal, const format1_tensor_t *format1)
{
    uint32_t h;
    uint32_t w;
    uint32_t c;

    for (h = 0U; h < normal->height; ++h) {
        for (w = 0U; w < normal->width; w += 97U) {
            for (c = 0U; c < normal->channel; c += 17U) {
                size_t idx = normal_offset(normal->height, normal->width, normal->channel, c, h, w);
                int16_t from_format1 = format1_read_i16(format1, c, h, w);

                if (normal->data[idx] != from_format1) {
                    fprintf(stderr,
                            "format1 sample mismatch h=%u w=%u c=%u normal=%d format1=%d\n",
                            h,
                            w,
                            c,
                            normal->data[idx],
                            from_format1);
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int validate_case(const data_node_t *input_node,
                         const data_node_t *output_node,
                         const ln_param_t *param,
                         const blob_t *input,
                         const blob_t *golden)
{
    size_t normal_bytes;
    size_t format1_bytes;
    uint32_t addr_delta;

    printf("[CHECK] begin\n");
    if ((input_node->format != 1U) || (output_node->format != 1U)) {
        fprintf(stderr, "[CHECK] format must be 1, input=%u output=%u\n",
                input_node->format,
                output_node->format);
        return -1;
    }

    if ((input_node->height != output_node->height) ||
        (input_node->width != output_node->width) ||
        (input_node->channel != output_node->channel)) {
        fprintf(stderr, "[CHECK] shape mismatch input=[%u,%u,%u] output=[%u,%u,%u]\n",
                input_node->height,
                input_node->width,
                input_node->channel,
                output_node->height,
                output_node->width,
                output_node->channel);
        return -2;
    }

    if (param->axis != 2) {
        fprintf(stderr, "[CHECK] axis should be 2 for shape [1,577,192], got=%d\n", param->axis);
        return -3;
    }

    if ((param->scale_len != input_node->channel) || (param->bias_len != input_node->channel)) {
        fprintf(stderr, "[CHECK] scale/bias len mismatch scale=%u bias=%u channel=%u\n",
                param->scale_len,
                param->bias_len,
                input_node->channel);
        return -4;
    }

    if ((param->x_radix_num != 1U) && (param->x_radix_num < input_node->channel)) {
        fprintf(stderr, "[CHECK] x_radix_num=%u cannot cover channel=%u\n",
                param->x_radix_num,
                input_node->channel);
        return -5;
    }

    if ((param->y_radix_num != 1U) && (param->y_radix_num < output_node->channel)) {
        fprintf(stderr, "[CHECK] y_radix_num=%u cannot cover channel=%u\n",
                param->y_radix_num,
                output_node->channel);
        return -6;
    }

    normal_bytes = (size_t)input_node->height * input_node->width * input_node->channel * sizeof(int16_t);
    if (input->size != normal_bytes) {
        fprintf(stderr, "[CHECK] input.bin size mismatch got=%zu expect=%zu\n", input->size, normal_bytes);
        return -7;
    }
    if (golden->size != normal_bytes) {
        fprintf(stderr, "[CHECK] golden.bin size mismatch got=%zu expect=%zu\n", golden->size, normal_bytes);
        return -8;
    }

    format1_bytes = format1_element_count(input_node->height, input_node->width, input_node->channel) * sizeof(int16_t);
    addr_delta = output_node->raw_addr - input_node->raw_addr;
    if (addr_delta != format1_bytes) {
        fprintf(stderr, "[CHECK] output-input addr delta mismatch got=0x%08x expect=0x%08zx\n",
                addr_delta,
                format1_bytes);
        return -9;
    }

    printf("[CHECK] PASS normal_bytes=%zu format1_bytes=%zu addr_delta=0x%08x\n",
           normal_bytes,
           format1_bytes,
           addr_delta);
    return 0;
}

int main(int argc, char **argv)
{
    const char *case_dir = (argc >= 2) ? argv[1] : "cases/vit_ln_v3/case_ncf";
    char path[1024];
    blob_t opflow;
    blob_t weight;
    blob_t input;
    blob_t golden;
    ln_node_t ln;
    data_node_t input_node;
    data_node_t output_node;
    ln_param_t param;
    normal_tensor_t input_tensor;
    format1_tensor_t input_format1;
    format1_tensor_t output_format1;
    char diff_report_path[1024];
    uint8_t *result;
    size_t result_size;
    int ret;

    make_path(path, sizeof(path), case_dir, "opflow.bin");
    opflow = read_file(path);
    make_path(path, sizeof(path), case_dir, "weight.bin");
    weight = read_file(path);
    make_path(path, sizeof(path), case_dir, "input.bin");
    input = read_file(path);
    make_path(path, sizeof(path), case_dir, "golden.bin");
    golden = read_file(path);

    if ((opflow.data == NULL) || (weight.data == NULL) || (input.data == NULL) || (golden.data == NULL)) {
        return 1;
    }

    ret = parse_opflow(&opflow, &ln, &input_node, &output_node);
    if (ret != 0) {
        fprintf(stderr, "parse opflow failed ret=%d\n", ret);
        return 1;
    }
    ret = parse_weight(&weight, &param);
    if (ret != 0) {
        fprintf(stderr, "parse weight failed ret=%d\n", ret);
        return 1;
    }

    printf("[OPFLOW] input idx=%u fmt=%u h=%u w=%u c=%u raw_addr=0x%08x\n",
           input_node.index,
           input_node.format,
           input_node.height,
           input_node.width,
           input_node.channel,
           input_node.raw_addr);
    printf("[OPFLOW] output idx=%u fmt=%u h=%u w=%u c=%u raw_addr=0x%08x\n",
           output_node.index,
           output_node.format,
           output_node.height,
           output_node.width,
           output_node.channel,
           output_node.raw_addr);
    printf("[LN] axis=%d stash=%d epsilon=%g x_radix_num=%u y_radix_num=%u scale_len=%u bias_len=%u\n",
           param.axis,
           param.stash_type,
           param.epsilon,
           param.x_radix_num,
           param.y_radix_num,
           param.scale_len,
           param.bias_len);

    ret = validate_case(&input_node, &output_node, &param, &input, &golden);
    if (ret != 0) {
        fprintf(stderr, "case validation failed ret=%d\n", ret);
        return 1;
    }

    result_size = (size_t)input_node.height * input_node.width * input_node.channel * sizeof(int16_t);
    if (golden.size != result_size) {
        fprintf(stderr, "golden size mismatch: got=%zu expect=%zu\n", golden.size, result_size);
        return 1;
    }
    ret = load_normal_tensor(&input, &input_node, &input_tensor);
    if (ret != 0) {
        fprintf(stderr, "load normal input failed ret=%d\n", ret);
        return 1;
    }
    ret = init_format1_tensor(&input_node, &input_format1);
    if (ret != 0) {
        free_normal_tensor(&input_tensor);
        return 1;
    }
    ret = normal_to_format1(&input_tensor, &input_format1);
    if (ret != 0) {
        fprintf(stderr, "normal to format1 failed ret=%d\n", ret);
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        return 1;
    }
    ret = check_format1_roundtrip_sample(&input_tensor, &input_format1);
    if (ret != 0) {
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        return 1;
    }
    printf("[FMT1] input normal -> format1 OK\n");
    ret = init_format1_tensor(&output_node, &output_format1);
    if (ret != 0) {
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        return 1;
    }

    result = (uint8_t *)malloc(result_size);
    if (result == NULL) {
        fprintf(stderr, "result malloc failed\n");
        free_format1_tensor(&output_format1);
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        return 1;
    }

    ret = run_layernorm_format1_input(&input_node, &param, &input_format1, &output_format1);
    if (ret != 0) {
        fprintf(stderr, "run LN failed ret=%d\n", ret);
        free_format1_tensor(&output_format1);
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        free(result);
        return 1;
    }
    if (format1_to_normal_bytes(&output_format1, result) != 0) {
        fprintf(stderr, "output format1 to normal failed\n");
        free_format1_tensor(&output_format1);
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        free(result);
        return 1;
    }
    printf("[FMT1] output format1 -> normal OK\n");

    make_path(path, sizeof(path), case_dir, "result_host.bin");
    if (write_file(path, result, result_size) != 0) {
        free_format1_tensor(&output_format1);
        free_format1_tensor(&input_format1);
        free_normal_tensor(&input_tensor);
        free(result);
        return 1;
    }
    printf("[OUT] %s bytes=%zu\n", path, result_size);

    make_path(diff_report_path, sizeof(diff_report_path), case_dir, "diff_report.csv");
    ret = compare_result(result, &golden, &output_node, diff_report_path);
    if (ret == 0) {
        printf("PASS_EXACT\n");
    } else if (ret == 1) {
        printf("PASS_TOL1\n");
    } else {
        printf("FAIL\n");
    }

    free(result);
    free_format1_tensor(&output_format1);
    free_format1_tensor(&input_format1);
    free_normal_tensor(&input_tensor);
    free(opflow.data);
    free(weight.data);
    free(input.data);
    free(golden.data);
    return (ret == 2) ? 1 : 0;
}
