# Call this script everytime that the .gatt file is modified.
# Instead of compiling the .gatt file at compile time, we do it manually everytime we modify it.
# Reason: .gatt file does not change that much. We don't want to add more dependencies to projects that use Bluepad32.
python3 ../external/btstack/tool/compile_gatt.py ../src/components/bluepad32/bt/uni_bt_service.gatt ../src/components/bluepad32/include/bt/uni_bt_service.gatt.h
