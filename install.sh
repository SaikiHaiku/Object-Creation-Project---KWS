#!/bin/bash

echo "============================================"
echo "  OCP Installer - KitariosWebStudio - KWS"
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
cat > ocp.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
python3 main.py
EOF
chmod +x ocp.sh
echo "Launcher created: ocp.sh"

echo
echo "[4/4] Installation complete!"
echo
echo "============================================"
echo "  OCP is ready to use!"
echo "  Developed by KitariosWebStudio - KWS"
echo "============================================"
echo
echo "You can now run: ./ocp.sh"
echo "Or: python3 main.py"
