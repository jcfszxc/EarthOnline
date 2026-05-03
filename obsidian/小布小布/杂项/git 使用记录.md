
```shell
cd /mnt/disk001/jcfszxc/Projects/0.bak/1.jcfszxc.eye

git init

touch README.md

git add .

git commit -m "Initial commit"

git branch

git push -u origin main

cat > .gitignore << EOF # Byte-compiled / optimized / DLL files __pycache__/ *.py[cod] *$py.class # Logs *.log # Environment .env venv/ # IDE .vscode/ .idea/ # OS .DS_Store Thumbs.db EOF
```