#pragma once

#include <napi.h>
#include "mtmd.h"
#include "mtmd-helper.h"

Napi::Object InitMultimodal(Napi::Env env, Napi::Object exports);

Napi::Value addonInitMultimodalBitmapFromBuffer(const Napi::CallbackInfo& info);
Napi::Value addonGetMultimodalBitmapDimensions(const Napi::CallbackInfo& info);
Napi::Value addonSetMultimodalBitmapId(const Napi::CallbackInfo& info);
Napi::Value addonGetMultimodalBitmapId(const Napi::CallbackInfo& info);
Napi::Value addonCreateMultimodalBitmaps(const Napi::CallbackInfo& info);
Napi::Value addonAddBitmapToMultimodalBitmaps(const Napi::CallbackInfo& info);
Napi::Value addonGetMultimodalBitmapData(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalTokenize(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalGetTokens(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalEvaluateChunks(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalTokenizeAndEvaluate(const Napi::CallbackInfo& info);
Napi::Value addonInitMultimodalBitmapFromAudio(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalSupportsAudio(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalGetAudioBitrate(const Napi::CallbackInfo& info);

// Vision encoder state functions
Napi::Value addonMultimodalExtractVisionState(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalLoadVisionState(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalSerializeVisionState(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalDeserializeVisionState(const Napi::CallbackInfo& info);
