#define MINIAUDIO_IMPLEMENTATION

#include <iostream>
#include "miniaudio/miniaudio.h"
#include "SOUL/source/API/soul_patch/API/soul_patch.h"

using namespace soul::patch;

// SOUL data we need for each "tick"
//
struct UserData {
  PatchPlayer::Ptr player;
};

// Pull from miniaudio "tick", push to SOUL "tick"
//
void ma_ondata(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    // Setup SOUL RenderContext
    const int maxMidi = 1024; // TODO what is a good number for this?
    MIDIMessage incomingMIDI[maxMidi], outgoingMIDI[maxMidi];
    PatchPlayer::RenderContext context;
    context.numInputChannels = device->capture.channels;
    context.numOutputChannels = device->playback.channels;
    context.incomingMIDI = incomingMIDI;
    context.outgoingMIDI = outgoingMIDI;
    context.numMIDIMessagesIn = 0;
    context.numMIDIMessagesOut = 0;
    context.maximumMIDIMessagesOut = maxMidi;

    // Convert from miniaudio  [ l0, r0, l1, r1, ... ]
    //                to SOUL  [[ l0, l1, ... ], [ r0, r1, ... ]]
    auto inputArray = *((float(*)[]) input);
    auto outputArray = *((float(*)[]) output);
    const float* inputChannels[device->capture.channels][frameCount];
    const float* outputChannels[device->playback.channels][frameCount];
    for (int i = 0; i < frameCount; i++)
      for (int j = 0; j < device->capture.channels; j++)
        inputChannels[j][i] = &inputArray[i*2 + j];
    for (int i = 0; i < frameCount; i++)
      for (int j = 0; j < device->playback.channels; j++)
        outputChannels[j][i] = &outputArray[i*2 + j];
    context.outputChannels = (float* const*) outputChannels;
    context.inputChannels = (const float* const*) inputChannels;
    context.numFrames = frameCount;

    // SOUL "tick"
    UserData* userData = (UserData (*)) device->pUserData;
    userData->player->render(context);
}

int main() {
    // Setup miniaudio
    UserData userData;
    ma_device device;
    ma_device_config audioConfig = ma_device_config_init(ma_device_type_duplex);
    audioConfig.dataCallback = ma_ondata;
    audioConfig.playback.format = ma_format_f32;
    audioConfig.pUserData = &userData;
    ma_device_init(NULL, &audioConfig, &device);

    // Setup SOUL
    SOULPatchLibrary library("/usr/local/lib/SOUL_PatchLoader.dylib");
    PatchInstance::Ptr patch = library.createPatchFromFileBundle("echo.soulpatch");
    PatchPlayerConfiguration playerConfig;
    playerConfig.sampleRate = device.sampleRate;
    playerConfig.maxFramesPerBlock = 512; // TODO what is a good number for this?
    userData.player = patch->compileNewPlayer(playerConfig, NULL, NULL, NULL, NULL);

    // Run device until any character is pressed
    ma_device_start(&device);
    std::cout << "Press any key to quit...";
    std::getchar();
    ma_device_uninit(&device);
}
