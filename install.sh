#!/bin/bash

echo "============================================"
echo "  BloxBot Installer"
echo "============================================"
echo

echo "[1/4] Checking Python..."
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python3 not found!"
    echo "Install Python 3.10+ with:"
    echo "  sudo apt install python3 python3-pip"
    exit 1
fi
echo "Python found!"

echo
echo "[2/4] Installing dependencies..."
pip3 install -r requirements.txt
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install dependencies!"
    exit 1
fi
echo "Dependencies installed!"

echo
echo "[3/4] Creating launcher script..."
cat > bloxbot.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
python3 main.py
EOF
chmod +x bloxbot.sh
echo "Launcher created: bloxbot.sh"

echo
echo "[4/4] Installation complete!"
echo
echo "============================================"
echo "  BloxBot is ready to use!"
echo "============================================"
echo
echo "You can now run: ./bloxbot.sh"
echo "Or: python3 main.py"
