#include <MvCameraControl.h>

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    const std::string wantedIp = argc > 1 ? argv[1] : std::string{};
    int result = MV_CC_Initialize();
    std::printf("MV_CC_Initialize=0x%08x\n", static_cast<unsigned int>(result));
    if (result != MV_OK) return 1;

    MV_CC_DEVICE_INFO_LIST devices{};
    const unsigned int layer = argc > 2 && std::string(argv[2]) == "usb" ? MV_USB_DEVICE : MV_GIGE_DEVICE;
    result = MV_CC_EnumDevices(layer, &devices);
    std::printf("MV_CC_EnumDevices=0x%08x count=%u\n", static_cast<unsigned int>(result), devices.nDeviceNum);
    if (result != MV_OK) {
        MV_CC_Finalize();
        return 2;
    }

    int matched = -1;
    for (unsigned int i = 0; i < devices.nDeviceNum; ++i) {
        const auto* device = devices.pDeviceInfo[i];
        if (!device) continue;
        if (device->nTLayerType != MV_GIGE_DEVICE) {
            std::printf("device[%u] transport=0x%08x skipped\n", i, device->nTLayerType);
            continue;
        }
        const auto& info = device->SpecialInfo.stGigEInfo;
        const unsigned int ip = info.nCurrentIp;
        char ipText[32]{};
        std::snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
        std::printf("device[%u] ip=%s vendor=%s model=%s serial=%s\n", i, ipText,
            reinterpret_cast<const char*>(info.chManufacturerName), reinterpret_cast<const char*>(info.chModelName),
            reinterpret_cast<const char*>(info.chSerialNumber));
        if (wantedIp.empty() || wantedIp == ipText) matched = static_cast<int>(i);
    }

    if (matched < 0) {
        std::printf("target device not enumerated; no open/grab attempted\n");
        MV_CC_Finalize();
        return 3;
    }

    void* handle{};
    result = MV_CC_CreateHandle(&handle, devices.pDeviceInfo[matched]);
    std::printf("MV_CC_CreateHandle=0x%08x\n", static_cast<unsigned int>(result));
    if (result != MV_OK) { MV_CC_Finalize(); return 4; }

    bool opened = false;
    bool grabbing = false;
    result = MV_CC_OpenDevice(handle, MV_ACCESS_Control, 0);
    std::printf("MV_CC_OpenDevice(Control)=0x%08x\n", static_cast<unsigned int>(result));
    if (result == MV_OK) {
        opened = true;
        result = MV_CC_StartGrabbing(handle);
        std::printf("MV_CC_StartGrabbing=0x%08x\n", static_cast<unsigned int>(result));
        if (result == MV_OK) {
            grabbing = true;
            MV_FRAME_OUT frame{};
            result = MV_CC_GetImageBuffer(handle, &frame, 3000);
            std::printf("MV_CC_GetImageBuffer=0x%08x", static_cast<unsigned int>(result));
            if (result == MV_OK) {
                std::printf(" width=%u height=%u frame=%u length=%llu pixelType=0x%08x\n",
                    frame.stFrameInfo.nWidth, frame.stFrameInfo.nHeight, frame.stFrameInfo.nFrameNum,
                    static_cast<unsigned long long>(frame.stFrameInfo.nFrameLenEx),
                    static_cast<unsigned int>(frame.stFrameInfo.enPixelType));
                const int freeResult = MV_CC_FreeImageBuffer(handle, &frame);
                std::printf("MV_CC_FreeImageBuffer=0x%08x\n", static_cast<unsigned int>(freeResult));
                if (freeResult != MV_OK) result = freeResult;
            } else {
                std::printf("\n");
            }
        }
    }

    if (grabbing) {
        const int stopResult = MV_CC_StopGrabbing(handle);
        std::printf("MV_CC_StopGrabbing=0x%08x\n", static_cast<unsigned int>(stopResult));
        if (result == MV_OK && stopResult != MV_OK) result = stopResult;
    }
    if (opened) {
        const int closeResult = MV_CC_CloseDevice(handle);
        std::printf("MV_CC_CloseDevice=0x%08x\n", static_cast<unsigned int>(closeResult));
        if (result == MV_OK && closeResult != MV_OK) result = closeResult;
    }
    const int destroyResult = MV_CC_DestroyHandle(handle);
    std::printf("MV_CC_DestroyHandle=0x%08x\n", static_cast<unsigned int>(destroyResult));
    const int finalizeResult = MV_CC_Finalize();
    std::printf("MV_CC_Finalize=0x%08x\n", static_cast<unsigned int>(finalizeResult));
    if (result != MV_OK) return 5;
    return destroyResult == MV_OK && finalizeResult == MV_OK ? 0 : 6;
}