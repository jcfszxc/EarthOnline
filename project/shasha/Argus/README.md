# Project Argus

## 🌌 项目简介
**Argus** 时刻监视着您关注的直播流，自动完成**录制**、**网盘备份**，并通过 **QQ 机器人**实时通知您。

## 🗺️ 系统架构与进度

| 阶段 | 模块名称 | 功能描述 | 状态 |
| :--- | :--- | :--- | :--- |
| **Phase 1** | **NapCat Core** | QQ 协议登录、消息收发、OneBot 接口 | ✅ **已完成** |
| **Phase 2** | **Live Recorder** | 自动监测直播、录制视频流 | ✅ **已完成** |
| **Phase 3** | **Cloud Uploader** | 自动上传录像至网盘 (阿里/百度/WebDAV) | ✅ **已完成** |
| **Phase 4** | **Bilibili Monitor** | 监控B站用户动态、直播开播与下播通知 | ✅ **已完成** |
| **Phase 5** | **Web Dashboard** | 🚧 规划中 | 🚧 规划中 |


## 🚀 快速启动 (Quick Start)

### 1. 配置 NapCat Core

需要在服务器后台开放6099和3001端口。

创建napcat的docker，并且启动容器。

```shell
mkdir -p phase-1-napcat/
cd ~/Argus/phase-1-napcat

cat > docker-compose.yml << 'EOF'
version: '3.8'

services:
  napcat:
    image: mlikiowa/napcat-docker:latest
    container_name: argus-napcat
    restart: always
    ports:
      - "6099:6099"   # WebUI
      - "3001:3001"   # OneBot 接口
    volumes:
      - ./data:/app/napcat/config
    environment:
      - TZ=Asia/Shanghai
      - NAPCAT_HOST=0.0.0.0
    networks:
      - napcat-net

networks:
  napcat-net:
    driver: bridge
EOF

docker compose up -d

```

进入网址 http://[IP地址]:6099/webui/ 

查看docker日志里，获取登录所需要的token信息

```shell
docker-compose logs -f
```


### 2. 配置录播姬

创建目录，并且下载录播姬软件

```shell
mkdir -p phase-2-recorder/data/software/recorder_luboji
mkdir -p phase-2-recorder/data/recordings

cd phase-2-recorder//data/software/recorder_luboji

wget https://github.com/BililiveRecorder/BililiveRecorder/releases/latest/download/BililiveRecorder-CLI-linux-x64.zip

unzip BililiveRecorder-CLI-linux-x64.zip

./BililiveRecorder.Cli --version

chmod +x ./BililiveRecorder.Cli

alias brec="./BililiveRecorder.Cli"

brec run --bind "http://*:2356" --http-basic-user "用户名" --http-basic-pass "密码" "../../recordings/"
```

测试成功后，可以尝试部署Systemd 服务实现自启动（可选）：

1. 创建服务文件

```shell
sudo nano /etc/systemd/system/brec.service
```

2. 填入以下内容
(请根据您的实际路径调整 WorkingDirectory 和 ExecStart 中的路径)

```shell
[Unit]
Description=Argus Phase 2: BililiveRecorder (Luboji)
After=network.target

[Service]
Type=simple
User=root
Group=root

# 【工作目录】设置为软件所在的文件夹
WorkingDirectory=/root/Argus/phase-2-recorder/data/software/recorder_luboji

# 【启动命令】
# 1. 使用绝对路径调用 BililiveRecorder.Cli
# 2. 参数中的录制路径也建议使用绝对路径，避免歧义
ExecStart=/root/Argus/phase-2-recorder/data/software/recorder_luboji/BililiveRecorder.Cli run --bind "http://*:2356" --http-basic-user "用户名" --http-basic-pass "密码" "/root/Argus/phase-2-recorder/data/recordings/"

# 崩溃自动重启
Restart=always
RestartSec=5

# 日志记录
StandardOutput=journal
StandardError=journal

# 可选：限制打开的文件数 (录播可能需要大量文件句柄)
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```



保存并退出 (Ctrl+O, Enter, Ctrl+X)。

重载配置：
```shell
sudo systemctl daemon-reload
```

启动服务：
```shell
sudo systemctl start brec
```

查看状态（确保显示 Active: active (running)）：
```shell
sudo systemctl status brec
```

设置开机自启：
```shell
sudo systemctl enable brec
``` 
实时查看日志（确认是否有报错）：

```shell
sudo journalctl -u brec -f
``` 

配置成功后，打开网址：http://[IP地址]:2356/，进行更详细的配置。

详细查看 [桌面版安装教程](https://rec.danmuji.org/install/desktop/)




### 3. 配置百度网盘自动备份

这里使用的是 [https://github.com/qjfoidnh/BaiduPCS-Go/releases](https://github.com/qjfoidnh/BaiduPCS-Go/releases)，登录方式参考里面讲解。


```shell
mkdir -p phase-3-uploader/data/software
cd phase-3-uploader/data/software

wget https://github.com/qjfoidnh/BaiduPCS-Go/releases/download/v4.0.1/BaiduPCS-Go-v4.0.1-linux-amd64.zip
unzip BaiduPCS-Go-v4.0.1-linux-amd64.zip 

cd BaiduPCS-Go-v4.0.1-linux-amd64
[登录百度账号]

cd ../../../
python baidu_upload_monitor.py 
```



测试成功后，可以尝试部署Systemd 服务实现自启动（可选）：

1. 创建服务文件

```shell
sudo nano /etc/systemd/system/baidu-upload-monitor.service
```

2. 填入以下内容
(请根据您的实际路径调整路径)

```shell
[Unit]
Description=Baidu Upload Monitor Service
After=network.target

[Service]
# 请修改为你的项目所在目录
WorkingDirectory=/root/Argus/phase-3-uploader
# 请确保这里的路径是你的 python 解释器的绝对路径
ExecStart=/usr/bin/python3 /root/Argus/phase-3-uploader/baidu_upload_monitor.py
# 如果程序崩溃，自动重启
Restart=on-failure
# 设置运行服务的用户，建议使用非root用户以提高安全性
User=root
Group=root

[Install]
WantedBy=multi-user.target
```

保存并退出 (Ctrl+O, Enter, Ctrl+X)。

重载配置：
```shell
sudo systemctl daemon-reload
```

启动服务：
```shell
sudo systemctl start baidu-upload-monitor.service
```

查看状态（确保显示 Active: active (running)）：
```shell
sudo systemctl status baidu-upload-monitor.service
```

设置开机自启：
```shell
sudo systemctl enable baidu-upload-monitor.service
``` 
实时查看日志（确认是否有报错）：

```shell
sudo journalctl -u baidu-upload-monitor.service -f
``` 

停止服务
```shell
sudo systemctl stop baidu-upload-monitor.service
```




### 4. 配置B站动态和直播监控通知

根据phase的步骤先登录http://[IP地址]:6099/的QQ

登陆后，进入网络配置->新建->Websocket服务器
```shell
设置为启用
名称随便写，我写的是BiliBili Monitor
Host: 0.0.0.0
填入token
确认
```

配置B站动态和直播监控通知

```shell
mkdir phase-4-monitor
cd phase-4-monitor
# docker pull menghuanan/dynamic-bot:v1.7.44
mkdir -p config data temp logs

nano docker-compose.yml

```

输入这些内容

```shell
version: '3.8'

services:
  dynamic-bot:
    image: menghuanan/dynamic-bot:v1.7.43
    container_name: dynamic-bot
    restart: unless-stopped
    environment:
      - TZ=Asia/Shanghai
    volumes:
      - ./config:/app/config
      - ./data:/app/data
      - ./temp:/app/temp
      - ./logs:/app/logs
    network_mode: "bridge"  # 使用 bridge 网络模式
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
```


启动容器：

```shell
docker-compose up -d
```


首次运行后，编辑 config/bot.yml：

我的是阿里云服务器，根据自己服务器的实际IP进行修改

修改 host 为 172.17.0.1，first_run_flag 改为 1

token: ""     #如果有则填入，没有不填


```shell
docker-compose down

docker-compose up -d

docker-compose logs -f
```




### 5. 配置网站


```shell
# 确认系统 Python 版本
python3 --version

# 重建 venv
python3 -m venv venv

# 激活
source venv/bin/activate

# 安装后端依赖
pip install fastapi uvicorn

mkdir -p backend/data frontend
touch backend/app.py backend/data/projects.json requirements.txt

# 用 nvm 升级 Node
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
source ~/.bashrc
nvm install 20
nvm use 20
node --version  # 应该显示 v20.x.x

cd ~/Argus/phase-5-website/frontend
npm create vite@latest . -- --template vue
# 遇到提示选 y（确认在当前目录）
npm install

cd ~/Argus/phase-5-website/frontend
npm run build

# 装 Nginx
apt update && apt install -y nginx

# 创建网站目录
mkdir -p /var/www/argus

# 把 build 产物复制过去
cp -r ~/Argus/phase-5-website/frontend/dist/* /var/www/argus/


cat > /etc/nginx/sites-available/argus << 'EOF'
server {
    listen 80;
    server_name 你的域名;

    root /var/www/argus;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }
}
EOF

# 启用配置
ln -s /etc/nginx/sites-available/argus /etc/nginx/sites-enabled/
nginx -t
systemctl reload nginx


```