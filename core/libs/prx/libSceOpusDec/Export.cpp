#include "prx/libSceOpusDec/include/Decoder.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context) {
    return OpusDec::Initialize_nid_no_patch(context);
}

int APS5_VABI sceOpusDecTerminate(std::uint32_t* context) {
    return OpusDec::Terminate_nid_no_patch(context);
}

int APS5_VABI sceOpusDecGetSize(int channels) {
    return OpusDec::GetSize_nid_no_patch(channels);
}

int APS5_VABI sceOpusDecCreateEx(std::uint32_t* context, void* state, int sampleRate, int channels) {
    return OpusDec::Create_nid_no_patch(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* packet, int packetBytes, std::int16_t* pcm, int capacityBytes) {
    return OpusDec::Decode_nid_no_patch(state, packet, packetBytes, pcm, capacityBytes, false);
}

int APS5_VABI sceOpusDecDestroy(void* state) {
    return OpusDec::Destroy_nid_no_patch(state);
}

}
