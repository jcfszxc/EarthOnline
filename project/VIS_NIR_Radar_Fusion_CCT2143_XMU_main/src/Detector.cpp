/**
 * Detector.cpp - RKNN 神经网络目标检测器实现
 *
 * 本文件实现了基于 RKNN (Rockchip Neural Network) SDK 的目标检测器。
 * 主要功能包括：
 * 1. 模型加载、初始化、参数设置和资源管理
 * 2. 主要检测函数 - 处理输入图像数据并返回检测结果
 * 3. 文件读取函数 - 用于加载标签和配置
 * 4. 量化/反量化函数 - 用于数据转换
 * 5. 排序和NMS算法 - 用于过滤和排序检测结果
 * 6. 检测结果处理函数 - 处理模型的原始输出并生成结构化的检测结果
 * 7. 两种处理路径 - 分别支持量化模型和浮点模型的处理
 *
 * 该检测器用于嵌入式设备上的实时目标检测应用。
 */

#include "Detector.h"

// 构造函数，初始化检测器并加载模型
Detector::Detector(const Config &config)
    : config_(config), ctx_(0) // 初始化配置和RKNN上下文
{
    initializeRKNN(); // 调用初始化函数
}

// 析构函数，释放资源
Detector::~Detector()
{
    if (ctx_)
    {
        // rknn_destroy(ctx_); // 注释掉的销毁上下文代码
    }
}

/**
 * 从文件指针加载数据到内存
 * @param fp 文件指针
 * @param ofst 文件偏移量
 * @param sz 要读取的数据大小
 * @return 指向加载数据的指针，失败返回NULL
 */
unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
    unsigned char *data;
    int ret;

    data = NULL;

    // 检查文件指针是否有效
    if (NULL == fp)
    {
        return NULL;
    }

    // 将文件指针移动到指定偏移位置
    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        printf("blob seek failure.\n");
        return NULL;
    }

    // 分配内存用于存储数据
    data = (unsigned char *)malloc(sz);
    if (data == NULL)
    {
        printf("buffer malloc failure.\n");
        return NULL;
    }
    // 从文件读取数据到内存
    ret = fread(data, 1, sz, fp);
    return data;
}

/**
 * 从文件加载模型
 * @param filename 模型文件路径
 * @param model_size 输出参数，返回模型大小
 * @return 指向模型数据的指针，失败返回NULL
 */
unsigned char *load_model(const char *filename, int *model_size)
{
    FILE *fp;
    unsigned char *data;

    // 以二进制读取模式打开文件
    fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // 从文件开始位置加载所有数据
    data = load_data(fp, 0, size);

    // 关闭文件
    fclose(fp);

    // 设置模型大小并返回数据
    *model_size = size;
    return data;
}

/**
 * 打印张量属性信息
 * @param attr 指向张量属性的指针
 */
void dump_tensor_attr(rknn_tensor_attr *attr)
{
    // 创建张量维度的字符串表示
    std::string shape_str = attr->n_dims < 1 ? "" : std::to_string(attr->dims[0]);
    for (uint32_t i = 1; i < attr->n_dims; ++i)
    {
        shape_str += ", " + std::to_string(attr->dims[i]);
    }

    // 打印张量属性
    printf("  index=%d, name=%s, n_dims=%d, dims=[%s], n_elems=%d, size=%d, w_stride = %d, size_with_stride=%d, fmt=%s, "
           "type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, shape_str.c_str(), attr->n_elems, attr->size, attr->w_stride,
           attr->size_with_stride, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

/**
 * 初始化RKNN上下文和相关参数
 * 包括加载模型、查询SDK版本、设置输入输出参数等
 */
void Detector::initializeRKNN()
{
    std::cout << "Initializing RKNN context and detection parameters..." << std::endl;
    // ctx_ = initializeRKNN(config_.modelPath, io_num); // 注释掉的旧初始化代码
    int ret;

    // 加载模型数据
    printf("Loading model...\n");
    int model_data_size = 0;
    unsigned char *model_data = load_model(config_.getRKNNModelPath().c_str(), &model_data_size);

    // 初始化RKNN上下文
    ret = rknn_init(&ctx_, model_data, model_data_size, 0, NULL);
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        exit(-1);
    }

    // 查询SDK版本信息
    rknn_sdk_version version;
    ret = rknn_query(ctx_, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0)
    {
        printf("rknn_query error ret=%d\n", ret);
        exit(-1);
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    // 查询模型输入/输出数量
    ret = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num_, sizeof(io_num_));
    if (ret < 0)
    {
        printf("rknn_query error ret=%d\n", ret);
        exit(-1);
    }
    printf("model input num: %d, output num: %d\n", io_num_.n_input, io_num_.n_output);

    // 根据输入/输出数量调整向量大小
    input_attrs_.resize(io_num_.n_input);
    output_attrs_.resize(io_num_.n_output);

    // 查询每个输入张量的属性
    for (uint32_t i = 0; i < io_num_.n_input; i++)
    {
        input_attrs_[i].index = i;
        ret = rknn_query(ctx_, RKNN_QUERY_INPUT_ATTR, &(input_attrs_[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            printf("rknn_query error ret=%d\n", ret);
            exit(-1);
        }
        dump_tensor_attr(&(input_attrs_[i]));
    }

    // 查询每个输出张量的属性
    for (uint32_t i = 0; i < io_num_.n_output; i++)
    {
        output_attrs_[i].index = i;
        ret = rknn_query(ctx_, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs_[i]), sizeof(rknn_tensor_attr));
        dump_tensor_attr(&(output_attrs_[i]));
    }

    std::cout << "Tensor attributes queried successfully." << std::endl;

    // 设置模型输入参数
    channel_ = 3;                            // RGB三通道
    model_height_ = input_attrs_[0].dims[2]; // 模型输入高度
    model_width_ = input_attrs_[0].dims[1];  // 模型输入宽度
    printf("model input height=%d, width=%d, channel=%d\n", model_height_, model_width_, channel_);

    // 创建检测参数（注释掉的代码）
    // DetectionParams params = createDetectionParams(ctx_, io_num_, input_attrs_, output_attrs_,
    //                                                model_width_, model_height_, config_.getBoxThresh(), config_.getNmsThresh());

    // 打印检测阈值
    std::cout << "Detection thresholds - Box Threshold: " << config_.getBoxThresh() << ", NMS Threshold: " << config_.getNmsThresh() << std::endl;

    // 设置目标类别数量和标签
    obj_class_num_ = config_.getObjClassNum();
    labels_.resize(obj_class_num_);
    prop_box_size_ = 5 + obj_class_num_; // 包含位置信息和每个类别概率

    std::cout << "Detection parameters created." << std::endl;
}

// 添加点在多边形内部判断函数（如果尚未添加）
/**
 * 判断点是否在多边形内部
 * @param point_x 点的X坐标
 * @param point_y 点的Y坐标
 * @param polygon 多边形顶点数组
 * @return 如果点在多边形内返回true，否则返回false
 */
bool isPointInPolygon(int point_x, int point_y, const std::vector<std::pair<int, int>>& polygon) {
    bool inside = false;
    int vertices = polygon.size();
    
    // 至少需要3个顶点才能形成一个多边形
    if (vertices < 3) {
        return false;
    }
    
    // 射线法判断点是否在多边形内
    for (int i = 0, j = vertices - 1; i < vertices; j = i++) {
        // 判断射线是否与多边形边相交
        if (((polygon[i].second > point_y) != (polygon[j].second > point_y)) &&
            (point_x < (polygon[j].first - polygon[i].first) * (point_y - polygon[i].second) / 
                        (polygon[j].second - polygon[i].second) + polygon[i].first)) {
            inside = !inside;
        }
    }
    
    return inside;
}

/**
 * Detector.cpp (续) - RKNN 检测器核心检测和辅助函数
 *
 * 本部分包含目标检测的核心实现，包括：
 * 1. 主要检测函数 - 处理输入图像数据并返回检测结果
 * 2. 文件读取函数 - 用于加载标签和配置
 * 3. 量化/反量化函数 - 用于数据转换
 */

/**
 * 执行目标检测的主函数
 * @param frame_data 输入图像数据
 * @return 检测结果组，包含所有检测到的目标
 */
detect_result_group_t Detector::detect(unsigned char *frame_data)
{
    // 设置RKNN输入参数
    rknn_input inputs[1];
    inputs[0].index = 0;                               // 输入索引
    inputs[0].type = RKNN_TENSOR_UINT8;                // 输入类型：无符号8位整数
    inputs[0].size = model_width_ * model_height_ * 3; // 输入大小：宽x高x3通道
    inputs[0].fmt = RKNN_TENSOR_NHWC;                  // 输入格式：NHWC (批次-高-宽-通道)
    inputs[0].pass_through = 0;                        // 不启用直通模式
    inputs[0].buf = frame_data;                        // 输入数据缓冲区

    // 设置RKNN输入
    rknn_inputs_set(ctx_, io_num_.n_input, inputs);

    // 准备输出缓冲区
    rknn_output outputs[io_num_.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (uint32_t i = 0; i < io_num_.n_output; ++i)
    {
        outputs[i].index = i;
        outputs[i].want_float = !config_.isQuantizedModel(); // 根据模型类型决定是否需要浮点输出
    }

    // 执行模型推理
    rknn_run(ctx_, NULL);

    // 获取模型输出
    rknn_outputs_get(ctx_, io_num_.n_output, outputs, NULL);

    // 准备检测结果结构和缩放参数
    detect_result_group_t detect_result_group;
    std::vector<float> out_scales; // 输出缩放因子
    std::vector<int32_t> out_zps;  // 输出零点值

    // 收集所有输出层的缩放和零点参数
    for (uint32_t i = 0; i < io_num_.n_output; ++i)
    {
        out_scales.push_back(output_attrs_[i].scale);
        out_zps.push_back(output_attrs_[i].zp);
    }

    // 设置填充参数
    BOX_RECT pads = {0};
    // memset(&pads, 0, sizeof(BOX_RECT)); // 注释掉的代码，功能与上一行相同

    // 计算输入图像与模型输入尺寸的缩放比例
    float scale_w = model_width_ / static_cast<float>(config_.getImageWidth("vic"));
    float scale_h = model_height_ / static_cast<float>(config_.getImageHeight("vic"));

    // 根据模型类型执行不同的后处理
    if (config_.isQuantizedModel())
    {
        // 量化模型的后处理
        post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf,
                     model_height_, model_width_,
                     config_.getBoxThresh(), config_.getNmsThresh(),
                     pads, scale_w, scale_h, out_zps, out_scales,
                     &detect_result_group);
    }
    else
    {
        // 浮点模型的后处理
        post_process_float((float *)outputs[0].buf, (float *)outputs[1].buf,
                           (float *)outputs[2].buf, model_height_, model_width_,
                           config_.getBoxThresh(), config_.getNmsThresh(),
                           pads, scale_w, scale_h, &detect_result_group);
    }

    // 注释掉的调试代码
    // std::cout << "config_.isQuantizedModel()" << config_.isQuantizedModel() << std::endl;
    // post_process_float((float *)outputs[0].buf, (float *)outputs[1].buf,
    //                    (float *)outputs[2].buf, model_height_, model_width_,
    //                    config_.getBoxThresh(), config_.getNmsThresh(),
    //                    pads, scale_w, scale_h, &detect_result_group);

    // 注释掉的打印检测结果代码
    // std::cout << "Detection Results (" << detect_result_group.count << " objects detected):" << std::endl;
    // for (int i = 0; i < detect_result_group.count; i++)
    // {
    //     const detect_result_t &result = detect_result_group.results[i];
    //     int center_x = (result.box.left + result.box.right) / 2;
    //     int center_y = (result.box.top + result.box.bottom) / 2;
    //     std::cout << "[" << i << "] Object: " << result.name
    //               << ", Confidence: " << result.prop
    //               << ", Box: (" << result.box.left << ", " << result.box.top
    //               << ", " << result.box.right << ", " << result.box.bottom << ")"
    //               << ", Center: (" << center_x << ", " << center_y << ")"
    //               << std::endl;
    // }

    // // 创建过滤后的结果组
    // detect_result_group_t filtered_result_group;
    // filtered_result_group.id = detect_result_group.id;
    // filtered_result_group.count = 0;

    // // 从配置中获取中心点Y坐标的过滤阈值
    // float center_y_min = config_.getCenterYMin();
    // float center_y_max = config_.getCenterYMax();

    // std::string local_ip = config_.getLocalIP();

    // // 根据规则过滤检测结果
    // for (int i = 0; i < detect_result_group.count; i++)
    // {
    //     const detect_result_t &result = detect_result_group.results[i];

    //     // 计算目标中心坐标
    //     int center_x = (result.box.left + result.box.right) / 2;
    //     int center_y = (result.box.top + result.box.bottom) / 2;

    //     // 应用过滤逻辑:
    //     // - "person"类别不进行过滤
    //     // - 非"person"类别，如果中心点Y坐标小于阈值或大于阈值，则过滤掉

    //     bool keep_object = true;
    //     if (strcmp(result.name, "person") != 0)
    //     {
    //         if (center_y < center_y_min || center_y > center_y_max)
    //         {
    //             keep_object = false;
    //         }
    //     }

    //     if (local_ip == "192.168.127.100" || local_ip == "192.168.127.120")
    //     {
    //         // 获取配置中的多边形过滤区域
    //         const std::vector<std::pair<int, int>>& filter_polygon = config_.getFilterPolygon();
            
    //         // 只有当多边形区域非空时才进行过滤
    //         if (!filter_polygon.empty() && isPointInPolygon(center_x, center_y, filter_polygon)) {
    //             keep_object = false;  // 在多边形区域内的目标被过滤掉
    //         }
    //     }


    //     if (keep_object)
    //     {
    //         // 将保留的目标添加到过滤后的结果中
    //         filtered_result_group.results[filtered_result_group.count] = result;
    //         filtered_result_group.count++;

    //         // 注释掉的打印保留目标信息代码
    //         // std::cout << "[" << filtered_result_group.count - 1 << "] Object: " << result.name
    //         //           << ", Confidence: " << result.prop
    //         //           << ", Box: (" << result.box.left << ", " << result.box.top
    //         //           << ", " << result.box.right << ", " << result.box.bottom << ")"
    //         //           << ", Center: (" << center_x << ", " << center_y << ")"
    //         //           << std::endl;
    //     }
    //     else
    //     {
    //         // 注释掉的打印过滤掉目标信息代码
    //         // std::cout << "Filtered out: " << result.name
    //         //           << ", Center: (" << center_x << ", " << center_y << ")"
    //         //           << std::endl;
    //     }
    // }

    // // 用过滤后的结果替换原始结果
    // detect_result_group = filtered_result_group;

    // 释放RKNN输出
    rknn_outputs_release(ctx_, io_num_.n_output, outputs);

    return detect_result_group;
}

/**
 * 从文件读取一行数据
 * @param fp 文件指针
 * @param buffer 存储读取数据的缓冲区
 * @param len 输出参数，返回读取的字符数
 * @return 指向读取的行数据的指针，失败返回NULL
 */
char *readLine(FILE *fp, char *buffer, int *len)
{
    int ch;
    int i = 0;
    size_t buff_len = 0;

    // 分配初始缓冲区
    buffer = (char *)malloc(buff_len + 1);
    if (!buffer)
        return NULL; // 内存不足

    // 逐字符读取直到遇到换行符或文件结束
    while ((ch = fgetc(fp)) != '\n' && ch != EOF)
    {
        buff_len++;
        // 扩展缓冲区
        void *tmp = realloc(buffer, buff_len + 1);
        if (tmp == NULL)
        {
            free(buffer);
            return NULL; // 内存不足
        }
        buffer = (char *)tmp;

        // 存储字符
        buffer[i] = (char)ch;
        i++;
    }
    buffer[i] = '\0'; // 添加字符串结束符

    *len = buff_len; // 设置输出长度

    // 检测文件结束或错误
    if (ch == EOF && (i == 0 || ferror(fp)))
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/**
 * 从文件读取多行数据
 * @param fileName 文件名
 * @param lines 存储行数据的向量
 * @param max_line 最大读取行数
 * @return 成功读取的行数，失败返回-1
 */
int readLines(const char *fileName, std::vector<char *> &lines, int max_line)
{
    // 打开文件
    FILE *file = fopen(fileName, "r");
    if (file == nullptr)
    {
        printf("Open %s fail!\n", fileName);
        return -1;
    }

    char *line = nullptr;
    size_t len = 0;
    ssize_t read;

    // 逐行读取文件内容
    int num = 0;
    while ((read = getline(&line, &len, file)) != -1 && num < max_line)
    {
        // 移除行尾的回车和换行符
        line[strcspn(line, "\r\n")] = 0;
        // 复制行数据并存储
        lines[num] = strdup(line);
        num++;
    }

    // 释放资源
    free(line);
    fclose(file);

    return num;
}

/**
 * 加载标签名称
 * @param locationFilename 标签文件路径
 * @return 成功返回0
 */
int Detector::loadLabelName(const char *locationFilename)
{
    printf("loadLabelName %s\n", locationFilename);
    // 读取标签文件内容到labels_向量
    readLines(locationFilename, labels_, obj_class_num_);
    return 0;
}

/**
 * 将浮点值裁剪到指定范围
 * @param val 输入浮点值
 * @param min 最小值
 * @param max 最大值
 * @return 裁剪后的整数值
 */
inline static int32_t __clip(float val, float min, float max)
{
    float f = val <= min ? min : (val >= max ? max : val);
    return f;
}

/**
 * 将浮点值转换为量化的int8值
 * @param f32 输入浮点值
 * @param zp 零点偏移
 * @param scale 缩放因子
 * @return 量化后的int8值
 */
static int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
    float dst_val = (f32 / scale) + zp;
    int8_t res = (int8_t)__clip(dst_val, -128, 127);
    return res;
}

/**
 * 将量化的int8值转换回浮点值
 * @param qnt 量化的int8值
 * @param zp 零点偏移
 * @param scale 缩放因子
 * @return 反量化后的浮点值
 */
static float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}

/**
 * Detector.cpp (续) - RKNN 目标检测后处理函数
 *
 * 本文件包含目标检测的后处理实现，包括：
 * 1. 排序和NMS算法 - 用于过滤和排序检测结果
 * 2. 检测结果处理函数 - 处理模型的原始输出并生成结构化的检测结果
 * 3. 两种处理路径 - 分别支持量化模型和浮点模型的处理
 */

/**
 * 对输入数组进行逆序快速排序（按照分数从大到小排序）
 * @param input 输入分数数组
 * @param left 起始索引
 * @param right 结束索引
 * @param indices 索引数组，将根据input的排序一起调整
 * @return 最后一个排序元素的位置
 */
static int quick_sort_indice_inverse(std::vector<float> &input, int left, int right, std::vector<int> &indices)
{
    float key;     // 基准元素的值
    int key_index; // 基准元素的索引
    int low = left;
    int high = right;

    if (left < right)
    {
        // 选择第一个元素作为基准元素
        key_index = indices[left];
        key = input[left];

        while (low < high)
        {
            // 从右向左找到第一个大于基准值的元素
            while (low < high && input[high] <= key)
            {
                high--;
            }
            // 将找到的元素放到low位置
            input[low] = input[high];
            indices[low] = indices[high];

            // 从左向右找到第一个小于基准值的元素
            while (low < high && input[low] >= key)
            {
                low++;
            }
            // 将找到的元素放到high位置
            input[high] = input[low];
            indices[high] = indices[low];
        }

        // 将基准元素放到最终位置
        input[low] = key;
        indices[low] = key_index;

        // 递归排序左右两部分
        quick_sort_indice_inverse(input, left, low - 1, indices);
        quick_sort_indice_inverse(input, low + 1, right, indices);
    }
    return low;
}

/**
 * 计算两个矩形框的交并比(IoU)
 * @param xmin0, ymin0, xmax0, ymax0 第一个矩形框的坐标
 * @param xmin1, ymin1, xmax1, ymax1 第二个矩形框的坐标
 * @return 两个矩形框的IoU值 (0~1之间)
 */
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1,
                              float ymax1)
{
    // 计算交集区域的宽度和高度
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);

    // 计算交集面积
    float i = w * h;

    // 计算并集面积（两个矩形面积之和减去交集面积）
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) + (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;

    // 返回IoU（防止分母为0）
    return u <= 0.f ? 0.f : (i / u);
}

/**
 * 非极大值抑制(NMS)算法
 * @param validCount 有效检测框的数量
 * @param outputLocations 检测框位置数组
 * @param classIds 类别ID数组
 * @param order 排序后的索引数组
 * @param filterId 当前处理的类别ID
 * @param threshold NMS阈值
 * @return 0表示成功
 */
static int nms(int validCount, std::vector<float> &outputLocations, std::vector<int> classIds, std::vector<int> &order,
               int filterId, float threshold)
{
    // 遍历所有检测框
    for (int i = 0; i < validCount; ++i)
    {
        // 跳过已经被抑制的框或者不属于当前类别的框
        if (order[i] == -1 || classIds[i] != filterId)
        {
            continue;
        }

        int n = order[i];
        // 将当前框与后面的所有框比较
        for (int j = i + 1; j < validCount; ++j)
        {
            int m = order[j];
            // 跳过已经被抑制的框或者不属于当前类别的框
            if (m == -1 || classIds[i] != filterId)
            {
                continue;
            }

            // 计算两个框的坐标
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            // 计算IoU
            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            // 如果IoU大于阈值，则抑制该框（设置为-1）
            if (iou > threshold)
            {
                order[j] = -1;
            }
        }
    }
    return 0;
}

/**
 * 将值限制在指定范围内
 * @param val 输入值
 * @param min 最小值
 * @param max 最大值
 * @return 裁剪后的值
 */
inline static int clamp(float val, int min, int max) { return val > min ? (val < max ? val : max) : min; }

/**
 * 处理量化模型的单个尺度输出
 * @param input 输入数据
 * @param anchor 锚点数组
 * @param grid_h, grid_w 特征图的高度和宽度
 * @param height, width 模型输入的高度和宽度
 * @param stride 步长
 * @param boxes 输出的边界框数组
 * @param objProbs 输出的目标概率数组
 * @param classId 输出的类别ID数组
 * @param threshold 置信度阈值
 * @param zp 零点值
 * @param scale 缩放因子
 * @return 有效检测框的数量
 */
int Detector::process(int8_t *input, int *anchor, int grid_h, int grid_w, int height, int width, int stride,
                      std::vector<float> &boxes, std::vector<float> &objProbs, std::vector<int> &classId, float threshold,
                      int32_t zp, float scale)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    // 将浮点阈值转换为量化值
    int8_t thres_i8 = qnt_f32_to_affine(threshold, zp, scale);

    // 循环处理3个锚点
    for (int a = 0; a < 3; a++)
    {
        // 遍历特征图的每个位置
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                // 获取置信度值
                int8_t box_confidence = input[(prop_box_size_ * a + 4) * grid_len + i * grid_w + j];
                // 如果置信度大于阈值，则处理该检测框
                if (box_confidence >= thres_i8)
                {
                    // 计算当前检测框在输入数据中的偏移量
                    int offset = (prop_box_size_ * a) * grid_len + i * grid_w + j;
                    int8_t *in_ptr = input + offset;

                    // 解码边界框坐标并反量化
                    float box_x = (deqnt_affine_to_f32(*in_ptr, zp, scale)) * 2.0 - 0.5;
                    float box_y = (deqnt_affine_to_f32(in_ptr[grid_len], zp, scale)) * 2.0 - 0.5;
                    float box_w = (deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale)) * 2.0;
                    float box_h = (deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale)) * 2.0;

                    // 将坐标转换为实际图像坐标
                    box_x = (box_x + j) * (float)stride;
                    box_y = (box_y + i) * (float)stride;
                    box_w = box_w * box_w * (float)anchor[a * 2];     // 应用锚点宽度
                    box_h = box_h * box_h * (float)anchor[a * 2 + 1]; // 应用锚点高度
                    box_x -= (box_w / 2.0);                           // 转换为左上角x坐标
                    box_y -= (box_h / 2.0);                           // 转换为左上角y坐标

                    // 寻找最大类别概率
                    int8_t maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;
                    for (int k = 1; k < obj_class_num_; ++k)
                    {
                        int8_t prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs)
                        {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }

                    // 如果最大类别概率大于阈值，则添加到结果中
                    if (maxClassProbs > thres_i8)
                    {
                        // 计算最终概率（类别概率 * 置信度）并反量化
                        objProbs.push_back((deqnt_affine_to_f32(maxClassProbs, zp, scale)) * (deqnt_affine_to_f32(box_confidence, zp, scale)));
                        classId.push_back(maxClassId);
                        validCount++;

                        // 添加边界框坐标
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return validCount;
}

/**
 * 量化模型的后处理函数
 * @param input0, input1, input2 三个不同尺度的输出
 * @param model_in_h, model_in_w 模型输入的高度和宽度
 * @param conf_threshold 置信度阈值
 * @param nms_threshold NMS阈值
 * @param pads 填充值
 * @param scale_w, scale_h 缩放因子
 * @param qnt_zps 零点值数组
 * @param qnt_scales 缩放因子数组
 * @param group 检测结果组
 * @return 0表示成功，-1表示失败
 */
int Detector::post_process(int8_t *input0, int8_t *input1, int8_t *input2, int model_in_h, int model_in_w, float conf_threshold,
                           float nms_threshold, BOX_RECT pads, float scale_w, float scale_h, std::vector<int32_t> &qnt_zps,
                           std::vector<float> &qnt_scales, detect_result_group_t *group)
{
    // 第一次运行时加载标签文件
    static int init = -1;
    if (init == -1)
    {
        int ret = 0;
        ret = loadLabelName(config_.getLabelNameTxtPath().c_str());
        if (ret < 0)
        {
            return -1;
        }

        init = 0;
    }
    // 初始化结果组
    *group = detect_result_group_t();

    // 创建存储检测结果的数组
    std::vector<float> filterBoxes; // 边界框坐标
    std::vector<float> objProbs;    // 目标概率
    std::vector<int> classId;       // 类别ID

    // 处理不同尺度的特征图（小、中、大目标）
    // 处理stride 8（用于检测小目标）
    int stride0 = 8;
    int grid_h0 = model_in_h / stride0;
    int grid_w0 = model_in_w / stride0;
    int validCount0 = 0;

    validCount0 = process(input0, (int *)anchor0, grid_h0, grid_w0, model_in_h, model_in_w, stride0, filterBoxes, objProbs,
                          classId, conf_threshold, qnt_zps[0], qnt_scales[0]);

    // 处理stride 16（用于检测中等大小目标）
    int stride1 = 16;
    int grid_h1 = model_in_h / stride1;
    int grid_w1 = model_in_w / stride1;
    int validCount1 = 0;
    validCount1 = process(input1, (int *)anchor1, grid_h1, grid_w1, model_in_h, model_in_w, stride1, filterBoxes, objProbs,
                          classId, conf_threshold, qnt_zps[1], qnt_scales[1]);

    // 处理stride 32（用于检测大目标）
    int stride2 = 32;
    int grid_h2 = model_in_h / stride2;
    int grid_w2 = model_in_w / stride2;
    int validCount2 = 0;
    validCount2 = process(input2, (int *)anchor2, grid_h2, grid_w2, model_in_h, model_in_w, stride2, filterBoxes, objProbs,
                          classId, conf_threshold, qnt_zps[2], qnt_scales[2]);

    // 计算所有有效检测框的数量
    int validCount = validCount0 + validCount1 + validCount2;
    // 如果没有检测到目标，直接返回
    if (validCount <= 0)
    {
        return 0;
    }

    // 创建索引数组，用于排序
    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i)
    {
        indexArray.push_back(i);
    }

    // 根据置信度对检测框进行排序
    quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

    // 获取所有出现的类别集合
    std::set<int> class_set(std::begin(classId), std::end(classId));

    // 对每个类别分别进行NMS
    for (auto c : class_set)
    {
        nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);
    }

    // 收集最终的检测结果
    int last_count = 0;
    group->count = 0;

    /* 处理有效的检测目标 */
    for (int i = 0; i < validCount; ++i)
    {
        // 跳过被NMS抑制的检测框或超出最大限制的检测框
        if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE)
        {
            continue;
        }
        int n = indexArray[i];

        // 计算边界框坐标
        float x1 = filterBoxes[n * 4 + 0] - pads.left;
        float y1 = filterBoxes[n * 4 + 1] - pads.top;
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        int id = classId[n];
        float obj_conf = objProbs[i];

        // 转换为原始图像坐标并限制在有效范围内
        group->results[last_count].box.left = (int)(clamp(x1, 0, model_in_w) / scale_w);
        group->results[last_count].box.top = (int)(clamp(y1, 0, model_in_h) / scale_h);
        group->results[last_count].box.right = (int)(clamp(x2, 0, model_in_w) / scale_w);
        group->results[last_count].box.bottom = (int)(clamp(y2, 0, model_in_h) / scale_h);
        group->results[last_count].prop = obj_conf;

        // 复制类别标签
        char *label = labels_[id];
        strncpy(group->results[last_count].name, label, OBJ_NAME_MAX_SIZE);

        // 注释掉的打印结果代码
        // printf("result %2d: (%4d, %4d, %4d, %4d), %s\n", i, group->results[last_count].box.left,
        // group->results[last_count].box.top,
        //        group->results[last_count].box.right, group->results[last_count].box.bottom, label);
        last_count++;
    }
    group->count = last_count;

    return 0;
}

/**
 * 处理浮点模型的单个尺度输出
 * @param input 输入数据
 * @param anchor 锚点数组
 * @param grid_h, grid_w 特征图的高度和宽度
 * @param height, width 模型输入的高度和宽度
 * @param stride 步长
 * @param boxes 输出的边界框数组
 * @param objProbs 输出的目标概率数组
 * @param classId 输出的类别ID数组
 * @param threshold 置信度阈值
 * @return 有效检测框的数量
 */
int Detector::process_float(float *input, int *anchor, int grid_h, int grid_w, int height, int width, int stride,
                            std::vector<float> &boxes, std::vector<float> &objProbs, std::vector<int> &classId, float threshold)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;

    // 循环处理3个锚点
    for (int a = 0; a < 3; a++)
    {
        // 遍历特征图的每个位置
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                // 获取置信度值
                float box_confidence = input[(prop_box_size_ * a + 4) * grid_len + i * grid_w + j];

                // 如果置信度大于阈值，则处理该检测框
                if (box_confidence >= threshold)
                {
                    // 计算当前检测框在输入数据中的偏移量
                    int offset = (prop_box_size_ * a) * grid_len + i * grid_w + j;
                    float *in_ptr = input + offset;

                    // 直接使用浮点值解码边界框坐标
                    float box_x = (in_ptr[0] * 2.0 - 0.5);
                    float box_y = (in_ptr[grid_len] * 2.0 - 0.5);
                    float box_w = (in_ptr[2 * grid_len] * 2.0);
                    float box_h = (in_ptr[3 * grid_len] * 2.0);

                    // 将坐标转换为实际图像坐标
                    box_x = (box_x + j) * (float)stride;
                    box_y = (box_y + i) * (float)stride;
                    box_w = box_w * box_w * (float)anchor[a * 2];     // 应用锚点宽度
                    box_h = box_h * box_h * (float)anchor[a * 2 + 1]; // 应用锚点高度
                    box_x -= (box_w / 2.0);                           // 转换为左上角x坐标
                    box_y -= (box_h / 2.0);                           // 转换为左上角y坐标

                    // 寻找最大类别概率
                    float maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;

                    // 找出概率最大的类别
                    for (int k = 1; k < obj_class_num_; ++k)
                    {
                        float prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs)
                        {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }

                    // 如果最大类别概率大于阈值，则添加到结果中
                    if (maxClassProbs > threshold)
                    {
                        // 计算最终概率（类别概率 * 置信度）
                        objProbs.push_back(maxClassProbs * box_confidence);
                        classId.push_back(maxClassId);
                        validCount++;

                        // 添加边界框坐标
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return validCount;
}

/**
 * 浮点模型的后处理函数
 * @param input0, input1, input2 三个不同尺度的输出
 * @param model_in_h, model_in_w 模型输入的高度和宽度
 * @param conf_threshold 置信度阈值
 * @param nms_threshold NMS阈值
 * @param pads 填充值
 * @param scale_w, scale_h 缩放因子
 * @param group 检测结果组
 * @return 0表示成功，-1表示失败
 */
int Detector::post_process_float(float *input0, float *input1, float *input2,
                                 int model_in_h, int model_in_w,
                                 float conf_threshold, float nms_threshold,
                                 BOX_RECT pads, float scale_w, float scale_h,
                                 detect_result_group_t *group)
{
    // 第一次运行时加载标签文件
    static int init = -1;
    if (init == -1)
    {
        int ret = loadLabelName(config_.getLabelNameTxtPath().c_str());
        if (ret < 0)
        {
            return -1;
        }
        init = 0;
    }

    // 初始化结果组
    *group = detect_result_group_t();

    // 创建存储检测结果的数组
    std::vector<float> filterBoxes; // 边界框坐标
    std::vector<float> objProbs;    // 目标概率
    std::vector<int> classId;       // 类别ID

    // 处理不同尺度的特征图（小、中、大目标）
    // 处理stride 8（用于检测小目标）
    int stride0 = 8;
    int grid_h0 = model_in_h / stride0;
    int grid_w0 = model_in_w / stride0;
    int validCount0 = process_float(input0, (int *)anchor0, grid_h0, grid_w0,
                                    model_in_h, model_in_w, stride0,
                                    filterBoxes, objProbs, classId, conf_threshold);

    // 处理stride 16（用于检测中等大小目标）
    int stride1 = 16;
    int grid_h1 = model_in_h / stride1;
    int grid_w1 = model_in_w / stride1;
    int validCount1 = process_float(input1, (int *)anchor1, grid_h1, grid_w1,
                                    model_in_h, model_in_w, stride1,
                                    filterBoxes, objProbs, classId, conf_threshold);

    // 处理stride 32（用于检测大目标）
    int stride2 = 32;
    int grid_h2 = model_in_h / stride2;
    int grid_w2 = model_in_w / stride2;
    int validCount2 = process_float(input2, (int *)anchor2, grid_h2, grid_w2,
                                    model_in_h, model_in_w, stride2,
                                    filterBoxes, objProbs, classId, conf_threshold);

    // 计算所有有效检测框的数量
    int validCount = validCount0 + validCount1 + validCount2;
    // 如果没有检测到目标，直接返回
    if (validCount <= 0)
    {
        return 0;
    }

    // 根据置信度对检测结果进行排序
    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i)
    {
        indexArray.push_back(i);
    }
    quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

    // 对每个类别分别进行NMS
    std::set<int> class_set(std::begin(classId), std::end(classId));
    for (auto c : class_set)
    {
        nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);
    }

    // 收集最终的检测结果
    int last_count = 0;
    group->count = 0;

    for (int i = 0; i < validCount; ++i)
    {
        // 跳过被NMS抑制的检测框或超出最大限制的检测框
        if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE)
        {
            continue;
        }

        int n = indexArray[i];
        // 计算边界框坐标
        float x1 = filterBoxes[n * 4 + 0] - pads.left;
        float y1 = filterBoxes[n * 4 + 1] - pads.top;
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        int id = classId[n];
        float obj_conf = objProbs[i];

        // 转换为原始图像坐标并限制在有效范围内
        group->results[last_count].box.left = (int)(clamp(x1, 0, model_in_w) / scale_w);
        group->results[last_count].box.top = (int)(clamp(y1, 0, model_in_h) / scale_h);
        group->results[last_count].box.right = (int)(clamp(x2, 0, model_in_w) / scale_w);
        group->results[last_count].box.bottom = (int)(clamp(y2, 0, model_in_h) / scale_h);
        group->results[last_count].prop = obj_conf;

        // 复制类别标签
        char *label = labels_[id];
        strncpy(group->results[last_count].name, label, OBJ_NAME_MAX_SIZE);

        last_count++;
    }
    // 更新最终检测到的目标数量
    group->count = last_count;

    return 0;
}