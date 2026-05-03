
1. 下载好yolov5的官方代码并且配置好训练所需环境

```shell
conda create -n yolov5 python=3.12
conda activate yolov5
pip install ultralytics
git clone https://github.com/ultralytics/yolov5
cd yolov5
pip install -r requirements.txt
```


2. 可以使用git来记录代码版本


3. 使用测试数据集来测试训练是否可以正常运行
```shell
python train.py --data coco128.yaml --epochs 300 --weights yolov5s.pt --batch-size -1 --cache
```


4. 标注第一批数据集

> 本次训练类别有：person,fishing_boat,small_boat,cruise_ship,warship,marker_float,other_ship,not_interested


[[yolo数据集txt版本可视化代码]]
[[yolo数据集清理空txt标签]]

```shell
python train.py --data custom.yaml --epochs 300 --weights yolov5s.pt --batch-size -1 --cache
```











