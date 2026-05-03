
```python
import tkinter as tk

from tkinter import ttk, filedialog, messagebox

from PIL import Image, ImageTk, ImageDraw, ImageFont

import os

from pathlib import Path

  

class YOLOVisualizer:

    def __init__(self, root):

        self.root = root

        self.root.title("YOLO 数据集可视化工具")

        self.root.geometry("1200x800")

        self.dataset_path = None

        self.current_subset = "train"  # train, val, test

        self.image_files = []

        self.current_image_path = None

        self.class_names = []

        # 颜色列表用于不同类别

        self.colors = [

            '#FF0000', '#00FF00', '#0000FF', '#FFFF00', '#FF00FF',

            '#00FFFF', '#FFA500', '#800080', '#008000', '#FFC0CB'

        ]

        self.setup_ui()

    def setup_ui(self):

        # 顶部控制栏

        top_frame = tk.Frame(self.root)

        top_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        tk.Button(top_frame, text="选择数据集目录", command=self.select_dataset).pack(side=tk.LEFT, padx=5)

        tk.Label(top_frame, text="子集:").pack(side=tk.LEFT, padx=5)

        self.subset_var = tk.StringVar(value="train")

        subset_combo = ttk.Combobox(top_frame, textvariable=self.subset_var,

                                     values=["train", "val", "test"], width=10, state="readonly")

        subset_combo.pack(side=tk.LEFT, padx=5)

        subset_combo.bind("<<ComboboxSelected>>", self.on_subset_change)

        tk.Label(top_frame, text="类别名称(逗号分隔):").pack(side=tk.LEFT, padx=5)

        self.class_entry = tk.Entry(top_frame, width=40)

        self.class_entry.pack(side=tk.LEFT, padx=5)

        self.class_entry.insert(0, "person,car,dog,cat,bicycle")

        tk.Button(top_frame, text="更新类别", command=self.update_classes).pack(side=tk.LEFT, padx=5)

        # 主内容区域

        main_frame = tk.Frame(self.root)

        main_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 左侧文件列表

        left_frame = tk.Frame(main_frame, width=250)

        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, padx=5)

        tk.Label(left_frame, text="图片列表", font=("Arial", 12, "bold")).pack(pady=5)

        # 搜索框

        search_frame = tk.Frame(left_frame)

        search_frame.pack(fill=tk.X, pady=5)

        tk.Label(search_frame, text="搜索:").pack(side=tk.LEFT)

        self.search_var = tk.StringVar()

        self.search_var.trace('w', self.filter_files)

        tk.Entry(search_frame, textvariable=self.search_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)

        # 列表框和滚动条

        list_frame = tk.Frame(left_frame)

        list_frame.pack(fill=tk.BOTH, expand=True)

        scrollbar = tk.Scrollbar(list_frame)

        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.file_listbox = tk.Listbox(list_frame, yscrollcommand=scrollbar.set,

                                       selectmode=tk.SINGLE, font=("Courier", 9))

        self.file_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        scrollbar.config(command=self.file_listbox.yview)

        self.file_listbox.bind('<<ListboxSelect>>', self.on_file_select)

        # 统计信息

        self.stats_label = tk.Label(left_frame, text="总计: 0 张图片", font=("Arial", 9))

        self.stats_label.pack(pady=5)

        # 右侧画布

        right_frame = tk.Frame(main_frame)

        right_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)

        # 顶部区域：图片预览标题和统计图

        top_right_frame = tk.Frame(right_frame)

        top_right_frame.pack(fill=tk.X, pady=5)

        tk.Label(top_right_frame, text="图片预览", font=("Arial", 12, "bold")).pack(side=tk.LEFT, padx=5)

        # 统计图按钮

        stats_btn_frame = tk.Frame(top_right_frame)

        stats_btn_frame.pack(side=tk.RIGHT, padx=5)

        self.show_current_stats = tk.BooleanVar(value=True)

        tk.Checkbutton(stats_btn_frame, text="当前图片统计", variable=self.show_current_stats,

                      command=self.update_statistics).pack(side=tk.LEFT)

        self.show_dataset_stats = tk.BooleanVar(value=False)

        tk.Checkbutton(stats_btn_frame, text="数据集统计", variable=self.show_dataset_stats,

                      command=self.update_statistics).pack(side=tk.LEFT, padx=5)

        # 图片信息标签

        self.info_label = tk.Label(right_frame, text="", font=("Arial", 9), justify=tk.LEFT)

        self.info_label.pack(pady=2)

        # 主显示区域（图片和统计图）

        display_frame = tk.Frame(right_frame)

        display_frame.pack(fill=tk.BOTH, expand=True)

        # 画布和滚动条

        canvas_frame = tk.Frame(display_frame)

        canvas_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        h_scrollbar = tk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL)

        h_scrollbar.pack(side=tk.BOTTOM, fill=tk.X)

        v_scrollbar = tk.Scrollbar(canvas_frame)

        v_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.canvas = tk.Canvas(canvas_frame, bg='gray',

                               xscrollcommand=h_scrollbar.set,

                               yscrollcommand=v_scrollbar.set)

        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        h_scrollbar.config(command=self.canvas.xview)

        v_scrollbar.config(command=self.canvas.yview)

        # 右侧统计图画布

        stats_frame = tk.Frame(display_frame, width=300, bg='white', relief=tk.RIDGE, borderwidth=2)

        stats_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=5)

        stats_frame.pack_propagate(False)

        tk.Label(stats_frame, text="类别统计", font=("Arial", 11, "bold"), bg='white').pack(pady=5)

        self.stats_canvas = tk.Canvas(stats_frame, bg='white', highlightthickness=0)

        self.stats_canvas.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 存储当前图片的框信息

        self.current_boxes = []

        # 存储数据集统计信息

        self.dataset_stats = {}

        self.update_classes()

    def calculate_dataset_stats(self):

        """计算整个数据集的类别统计"""

        if not self.dataset_path:

            return {}

        label_dir = self.dataset_path / "labels" / self.current_subset

        if not label_dir.exists():

            return {}

        stats = {}

        label_files = list(label_dir.glob("*.txt"))

        for label_path in label_files:

            try:

                with open(label_path, 'r') as f:

                    for line in f:

                        parts = line.strip().split()

                        if len(parts) >= 5:

                            class_id = int(parts[0])

                            stats[class_id] = stats.get(class_id, 0) + 1

            except:

                continue

        return stats

    def draw_statistics(self):

        """绘制统计图"""

        self.stats_canvas.delete("all")

        if self.show_current_stats.get() and self.current_boxes:

            self.draw_current_image_stats()

        elif self.show_dataset_stats.get():

            if not self.dataset_stats:

                self.dataset_stats = self.calculate_dataset_stats()

            self.draw_dataset_statistics()

        else:

            # 显示提示信息

            canvas_width = self.stats_canvas.winfo_width()

            canvas_height = self.stats_canvas.winfo_height()

            if canvas_width > 1 and canvas_height > 1:

                self.stats_canvas.create_text(

                    canvas_width // 2, canvas_height // 2,

                    text="请选择统计类型\n或加载图片",

                    font=("Arial", 10),

                    fill='gray',

                    justify=tk.CENTER

                )

    def draw_current_image_stats(self):

        """绘制当前图片的类别统计"""

        if not self.current_boxes:

            return

        # 统计当前图片的类别数量

        class_counts = {}

        for class_id, _, _, _, _ in self.current_boxes:

            class_counts[class_id] = class_counts.get(class_id, 0) + 1

        self.draw_bar_chart(class_counts, "当前图片")

    def draw_dataset_statistics(self):

        """绘制数据集统计"""

        if not self.dataset_stats:

            return

        self.draw_bar_chart(self.dataset_stats, f"{self.current_subset.upper()} 子集")

    def draw_bar_chart(self, data, title):

        """绘制横向柱状图"""

        if not data:

            return

        canvas_width = self.stats_canvas.winfo_width()

        canvas_height = self.stats_canvas.winfo_height()

        if canvas_width <= 1 or canvas_height <= 1:

            return

        # 绘制标题

        self.stats_canvas.create_text(

            canvas_width // 2, 10,

            text=title,

            font=("Arial", 10, "bold"),

            fill='black'

        )

        # 计算绘图区域

        margin_top = 35

        margin_bottom = 30

        margin_left = 100  # 增加左边距以显示类别名称

        margin_right = 50  # 增加右边距以显示数量

        chart_width = canvas_width - margin_left - margin_right

        chart_height = canvas_height - margin_top - margin_bottom

        if chart_width <= 0 or chart_height <= 0:

            return

        # 排序数据

        sorted_data = sorted(data.items())

        num_classes = len(sorted_data)

        if num_classes == 0:

            return

        max_count = max(data.values())

        # 计算柱子高度和间距

        bar_height = max(15, min(30, chart_height / (num_classes * 1.3)))

        spacing = bar_height * 0.3

        # 绘制坐标轴

        # Y轴（左侧）

        self.stats_canvas.create_line(

            margin_left, margin_top,

            margin_left, margin_top + chart_height,

            width=2, fill='black'

        )

        # X轴（底部）

        self.stats_canvas.create_line(

            margin_left, margin_top + chart_height,

            margin_left + chart_width, margin_top + chart_height,

            width=2, fill='black'

        )

        # 绘制X轴刻度和网格线

        num_x_ticks = 5

        for i in range(num_x_ticks + 1):

            x = margin_left + (i * chart_width / num_x_ticks)

            value = int(i * max_count / num_x_ticks)

            # 刻度标签

            self.stats_canvas.create_text(

                x, margin_top + chart_height + 15,

                text=str(value),

                font=("Arial", 8),

                anchor=tk.N

            )

            # 网格线

            if i > 0:

                self.stats_canvas.create_line(

                    x, margin_top,

                    x, margin_top + chart_height,

                    fill='lightgray',

                    dash=(2, 2)

                )

        # 绘制横向柱状图

        total_height = num_classes * (bar_height + spacing)

        start_y = margin_top + (chart_height - total_height) / 2

        for idx, (class_id, count) in enumerate(sorted_data):

            y = start_y + idx * (bar_height + spacing)

            # 计算柱子宽度

            if max_count > 0:

                bar_width = (count / max_count) * chart_width

            else:

                bar_width = 0

            x_left = margin_left

            x_right = margin_left + bar_width

            # 选择颜色

            color = self.colors[class_id % len(self.colors)]

            # 绘制柱子

            self.stats_canvas.create_rectangle(

                x_left, y,

                x_right, y + bar_height,

                fill=color,

                outline='black',

                width=1

            )

            # 在柱子右侧显示数量

            self.stats_canvas.create_text(

                x_right + 5, y + bar_height / 2,

                text=str(count),

                font=("Arial", 9, "bold"),

                anchor=tk.W,

                fill='black'

            )

            # Y轴标签（类别名称）

            if class_id < len(self.class_names):

                label = self.class_names[class_id]

                # 限制标签长度

                if len(label) > 12:

                    label = label[:10] + ".."

            else:

                label = f"Class {class_id}"

            # 类别名称（左侧）

            self.stats_canvas.create_text(

                margin_left - 5, y + bar_height / 2,

                text=label,

                font=("Arial", 9),

                anchor=tk.E,

                fill='black'

            )

            # 类别ID（更小的字体，显示在名称下方或旁边）

            self.stats_canvas.create_text(

                margin_left - 5, y + bar_height / 2,

                text=f" [ID:{class_id}]",

                font=("Arial", 7),

                anchor=tk.W,

                fill='gray'

            )

        # 显示总计

        total = sum(data.values())

        self.stats_canvas.create_text(

            canvas_width // 2, canvas_height - 5,

            text=f"总计: {total} 个标注框",

            font=("Arial", 9, "bold"),

            fill='navy'

        )

    def update_statistics(self):

        """更新统计图显示"""

        if self.show_dataset_stats.get():

            self.dataset_stats = self.calculate_dataset_stats()

        self.draw_statistics()

    def select_dataset(self):

        path = filedialog.askdirectory(title="选择数据集根目录")

        if path:

            self.dataset_path = Path(path)

            # 验证目录结构

            if not (self.dataset_path / "images").exists() or not (self.dataset_path / "labels").exists():

                messagebox.showerror("错误", "所选目录不是有效的YOLO数据集!\n需要包含 images 和 labels 文件夹")

                return

            self.load_dataset()

    def on_subset_change(self, event=None):

        self.current_subset = self.subset_var.get()

        self.load_dataset()

    def update_classes(self):

        class_text = self.class_entry.get().strip()

        if class_text:

            self.class_names = [c.strip() for c in class_text.split(',')]

        else:

            self.class_names = []

    def load_dataset(self):

        if not self.dataset_path:

            return

        image_dir = self.dataset_path / "images" / self.current_subset

        if not image_dir.exists():

            messagebox.showwarning("警告", f"目录不存在: {image_dir}")

            return

        # 支持的图片格式

        extensions = ['.jpg', '.jpeg', '.png', '.bmp']

        self.image_files = []

        for ext in extensions:

            self.image_files.extend(list(image_dir.glob(f"*{ext}")))

            self.image_files.extend(list(image_dir.glob(f"*{ext.upper()}")))

        self.image_files.sort()

        # 清空当前统计

        self.current_boxes = []

        self.dataset_stats = {}

        self.refresh_file_list()

        # 如果选择了数据集统计，重新计算

        if self.show_dataset_stats.get():

            self.update_statistics()

        else:

            self.draw_statistics()

    def refresh_file_list(self):

        self.file_listbox.delete(0, tk.END)

        search_term = self.search_var.get().lower()

        displayed_files = []

        for img_path in self.image_files:

            if search_term in img_path.name.lower():

                self.file_listbox.insert(tk.END, img_path.name)

                displayed_files.append(img_path)

        self.stats_label.config(text=f"总计: {len(self.image_files)} 张图片 (显示: {len(displayed_files)})")

    def filter_files(self, *args):

        self.refresh_file_list()

    def on_file_select(self, event):

        selection = self.file_listbox.curselection()

        if not selection:

            return

        filename = self.file_listbox.get(selection[0])

        # 从原始文件列表中找到完整路径

        for img_path in self.image_files:

            if img_path.name == filename:

                self.current_image_path = img_path

                self.display_image()

                break

    def display_image(self):

        if not self.current_image_path or not self.current_image_path.exists():

            return

        # 读取图片

        try:

            image = Image.open(self.current_image_path)

        except Exception as e:

            messagebox.showerror("错误", f"无法打开图片: {e}")

            return

        # 读取对应的标签文件

        label_path = self.dataset_path / "labels" / self.current_subset / (self.current_image_path.stem + ".txt")

        boxes = []

        if label_path.exists():

            try:

                with open(label_path, 'r') as f:

                    for line in f:

                        parts = line.strip().split()

                        if len(parts) >= 5:

                            class_id = int(parts[0])

                            x_center = float(parts[1])

                            y_center = float(parts[2])

                            width = float(parts[3])

                            height = float(parts[4])

                            boxes.append((class_id, x_center, y_center, width, height))

            except Exception as e:

                print(f"读取标签文件错误: {e}")

        # 保存当前框信息用于统计

        self.current_boxes = boxes

        # 在图片上绘制检测框

        draw = ImageDraw.Draw(image)

        img_width, img_height = image.size

        try:

            font = ImageFont.truetype("arial.ttf", 20)

        except:

            font = ImageFont.load_default()

        for class_id, x_center, y_center, box_width, box_height in boxes:

            # 转换为像素坐标

            x1 = int((x_center - box_width / 2) * img_width)

            y1 = int((y_center - box_height / 2) * img_height)

            x2 = int((x_center + box_width / 2) * img_width)

            y2 = int((y_center + box_height / 2) * img_height)

            # 选择颜色

            color = self.colors[class_id % len(self.colors)]

            # 绘制矩形框

            draw.rectangle([x1, y1, x2, y2], outline=color, width=3)

            # 绘制标签

            if class_id < len(self.class_names):

                label = f"{self.class_names[class_id]} ({class_id})"

            else:

                label = f"Class {class_id}"

            # 绘制标签背景

            text_bbox = draw.textbbox((x1, y1-25), label, font=font)

            draw.rectangle([text_bbox[0]-2, text_bbox[1]-2, text_bbox[2]+2, text_bbox[3]+2],

                          fill=color)

            draw.text((x1, y1-25), label, fill='white', font=font)

        # 更新画布

        self.photo = ImageTk.PhotoImage(image)

        self.canvas.delete("all")

        self.canvas.create_image(0, 0, anchor=tk.NW, image=self.photo)

        self.canvas.config(scrollregion=self.canvas.bbox(tk.ALL))

        # 更新信息标签

        info_text = f"文件: {self.current_image_path.name} | 尺寸: {img_width}x{img_height} | 标注框: {len(boxes)}"

        self.info_label.config(text=info_text)

        # 更新统计图

        if self.show_current_stats.get():

            self.draw_statistics()

  

if __name__ == "__main__":

    root = tk.Tk()

    app = YOLOVisualizer(root)

    root.mainloop()
```