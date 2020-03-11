#!/bin/bash
cd "${0%/*}"

# start.sh
mkdir ./Vkav.app/Contents/Macos
echo "#!/bin/bash" > ./Vkav.app/Contents/Macos/start.sh
echo "cd \"\${0%/*}\"" >> ./Vkav.app/Contents/Macos/start.sh
echo "VK_ICD_FILENAMES=\"$1/macOS/etc/vulkan/icd.d/MoltenVK_icd.json\" ./vkav \"\$@\"" >> ./Vkav.app/Contents/Macos/start.sh
chmod +x ./Vkav.app/Contents/Macos/start.sh

# vkav
cp ../build/vkav ./Vkav.app/Contents/Macos

# config
mkdir ./Vkav.app/Contents/Resources/vkav
cp ../src/config ./Vkav.app/Contents/Resources/vkav

# modules
cp -r ../src/modules ./Vkav.app/Contents/Resources/vkav
