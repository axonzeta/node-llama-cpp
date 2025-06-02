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
Napi::Value addonMultimodalEvaluateChunks(const Napi::CallbackInfo& info);
Napi::Value addonMultimodalTokenizeAndEvaluate(const Napi::CallbackInfo& info);
