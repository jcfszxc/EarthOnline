

```python
import os
import shutil
from pathlib import Path

def clean_empty_labels(image_dir, label_dir, output_dir='待处理文件'):
    """
    清理空的标签文件和对应的图片，移动到待处理文件夹
    
    参数:
        image_dir: 图片文件夹路径
        label_dir: 标签文件夹路径
        output_dir: 输出文件夹名称（默认: 待处理文件）
    """
    
    # 创建输出文件夹
    output_image_dir = os.path.join(output_dir, 'images')
    output_label_dir = os.path.join(output_dir, 'labels')
    os.makedirs(output_image_dir, exist_ok=True)
    os.makedirs(output_label_dir, exist_ok=True)
    
    # 支持的图片格式
    image_extensions = ['.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.webp']
    
    # 统计信息
    empty_count = 0
    moved_files = []
    
    print("=" * 60)
    print("开始扫描空标签文件...")
    print("=" * 60)
    
    # 递归遍历标签文件夹
    for root, dirs, files in os.walk(label_dir):
        for label_file in files:
            if not label_file.endswith('.txt'):
                continue
                
            label_path = os.path.join(root, label_file)
            
            # 检查文件是否为空或只包含空白字符
            is_empty = False
            
            # 先检查文件大小
            if os.path.getsize(label_path) == 0:
                is_empty = True
            else:
                # 读取文件内容检查是否只有空白字符
                try:
                    with open(label_path, 'r', encoding='utf-8') as f:
                        content = f.read().strip()
                        if not content:  # 去除空白后如果为空
                            is_empty = True
                except:
                    # 尝试其他编码
                    try:
                        with open(label_path, 'r', encoding='gbk') as f:
                            content = f.read().strip()
                            if not content:
                                is_empty = True
                    except:
                        pass
            
            if is_empty:
                empty_count += 1
                base_name = os.path.splitext(label_file)[0]
                
                # 计算相对路径（保持子文件夹结构）
                rel_path = os.path.relpath(root, label_dir)
                
                # 在对应的图片文件夹中查找
                image_search_dir = os.path.join(image_dir, rel_path)
                
                # 查找对应的图片文件
                image_found = False
                for ext in image_extensions:
                    image_name = base_name + ext
                    image_path = os.path.join(image_search_dir, image_name)
                    
                    if os.path.exists(image_path):
                        # 创建输出子文件夹
                        output_image_subdir = os.path.join(output_image_dir, rel_path)
                        os.makedirs(output_image_subdir, exist_ok=True)
                        
                        # 移动图片文件
                        new_image_path = os.path.join(output_image_subdir, image_name)
                        shutil.move(image_path, new_image_path)
                        image_found = True
                        
                        print(f"✓ 已移动图片: {os.path.join(rel_path, image_name)}")
                        moved_files.append(('image', os.path.join(rel_path, image_name)))
                        break
                
                # 移动标签文件
                output_label_subdir = os.path.join(output_label_dir, rel_path)
                os.makedirs(output_label_subdir, exist_ok=True)
                new_label_path = os.path.join(output_label_subdir, label_file)
                shutil.move(label_path, new_label_path)
                print(f"✓ 已移动标签: {os.path.join(rel_path, label_file)}")
                moved_files.append(('label', os.path.join(rel_path, label_file)))
                
                if not image_found:
                    print(f"  ⚠ 警告: 未找到对应的图片文件")
                
                print("-" * 60)
    
    # 输出统计信息
    print("\n" + "=" * 60)
    print("清理完成!")
    print("=" * 60)
    print(f"共找到空标签文件: {empty_count} 个")
    print(f"已移动文件总数: {len(moved_files)} 个")
    print(f"文件已移动到: {os.path.abspath(output_dir)}")
    print("=" * 60)
    
    # 显示待处理文件列表（只显示前20个）
    if moved_files:
        print("\n待处理文件列表（前20个）:")
        print("-" * 60)
        for file_type, filename in moved_files[:20]:
            print(f"  [{file_type}] {filename}")
        if len(moved_files) > 20:
            print(f"  ... 还有 {len(moved_files) - 20} 个文件")
    
    return empty_count, moved_files


def verify_cleanup(image_dir, label_dir):
    """
    验证清理后的数据集完整性
    
    参数:
        image_dir: 图片文件夹路径
        label_dir: 标签文件夹路径
    """
    print("\n" + "=" * 60)
    print("验证数据集完整性...")
    print("=" * 60)
    
    image_extensions = ['.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.webp']
    
    # 递归获取所有图片和标签
    images = []
    labels = []
    
    for root, dirs, files in os.walk(image_dir):
        for f in files:
            if any(f.lower().endswith(ext) for ext in image_extensions):
                rel_path = os.path.relpath(os.path.join(root, f), image_dir)
                images.append(rel_path)
    
    for root, dirs, files in os.walk(label_dir):
        for f in files:
            if f.endswith('.txt'):
                rel_path = os.path.relpath(os.path.join(root, f), label_dir)
                labels.append(rel_path)
    
    # 检查每个图片是否有对应的标签
    missing_labels = []
    for img in images:
        base_name = os.path.splitext(img)[0]
        label_name = base_name + '.txt'
        if label_name not in labels:
            missing_labels.append(img)
    
    # 检查每个标签是否有对应的图片
    missing_images = []
    for label in labels:
        base_name = os.path.splitext(label)[0]
        has_image = False
        for ext in image_extensions:
            if base_name + ext in images:
                has_image = True
                break
        if not has_image:
            missing_images.append(label)
    
    print(f"剩余图片数量: {len(images)}")
    print(f"剩余标签数量: {len(labels)}")
    
    if missing_labels:
        print(f"\n⚠ 警告: {len(missing_labels)} 个图片缺少标签")
        print("前10个示例:")
        for img in missing_labels[:10]:
            print(f"  - {img}")
    if missing_images:
        print(f"⚠ 警告: {len(missing_images)} 个标签缺少图片")
        print("前10个示例:")
        for lbl in missing_images[:10]:
            print(f"  - {lbl}")
    
    if not missing_labels and not missing_images:
        print("\n✓ 数据集完整性检查通过!")
    
    print("=" * 60)


if __name__ == "__main__":
    # 配置路径（请根据实际情况修改）
    IMAGE_DIR = "images"      # 图片文件夹路径
    LABEL_DIR = "labels"      # 标签文件夹路径
    OUTPUT_DIR = "待处理文件"  # 输出文件夹路径
    
    # 检查文件夹是否存在
    if not os.path.exists(IMAGE_DIR):
        print(f"错误: 图片文件夹不存在: {IMAGE_DIR}")
        exit(1)
    
    if not os.path.exists(LABEL_DIR):
        print(f"错误: 标签文件夹不存在: {LABEL_DIR}")
        exit(1)
    
    # 执行清理
    empty_count, moved_files = clean_empty_labels(IMAGE_DIR, LABEL_DIR, OUTPUT_DIR)
    
    # 验证清理后的数据集
    if empty_count > 0:
        verify_cleanup(IMAGE_DIR, LABEL_DIR)
    else:
        print("\n未发现空标签文件，数据集无需清理。")
```