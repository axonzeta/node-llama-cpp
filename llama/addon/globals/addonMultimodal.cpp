// Standard library includes
#include <string>
#include <vector>
#include <iostream> // For debugging, remove in production

// Napi: Node.js API for native addons
#include <napi.h>
#include "mtmd.h" // Multimodal context header
#include "mtmd-helper.h" // Multimodal helper functions
#include "../AddonContext.h"

// External C functions are expected to be declared in the included "mtmd.h"

class MultiBitmap : public Napi::ObjectWrap<MultiBitmap> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);
        
        Napi::Function func = DefineClass(env, "MultiBitmap", {
            InstanceMethod("getData", &MultiBitmap::GetData),
            InstanceMethod("getDimensions", &MultiBitmap::GetDimensions),
            InstanceMethod("getId", &MultiBitmap::GetId),
            InstanceMethod("setId", &MultiBitmap::SetId),
            InstanceMethod("dispose", &MultiBitmap::Dispose)
        });
        
        constructor = Napi::Persistent(func); // constructor is Napi::FunctionReference
        constructor.SuppressDestruct();
        
        exports.Set("MultiBitmap", func);
        return exports;
    }

    // Constructor should be public for Napi::ObjectWrap
    MultiBitmap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<MultiBitmap>(info) {
        // Constructor is now called without arguments from addonInitMultimodalBitmapFromBuffer
        // The bitmap_wrapper.ptr will be set externally
    }

    // Destructor relies on mtmd::bitmap's destructor to free the underlying C bitmap
    ~MultiBitmap() = default;

    // Make bitmap_wrapper public for access by MultiBitmaps if absolutely necessary,
    // though it's better to pass data via methods or copy.
    // For AddBitmap, we'll access data via methods.
    mtmd::bitmap bitmap_wrapper;

// private: // Keep constructor public
    static Napi::FunctionReference constructor;
    
    Napi::Value GetData(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (!bitmap_wrapper.ptr) {
            Napi::Error::New(env, "Bitmap has been disposed or was not initialized").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        const unsigned char * data = mtmd_bitmap_get_data(bitmap_wrapper.ptr.get());
        uint32_t nx = mtmd_bitmap_get_nx(bitmap_wrapper.ptr.get());
        uint32_t ny = mtmd_bitmap_get_ny(bitmap_wrapper.ptr.get());
        size_t size = (size_t)nx * ny * 3; // Assuming 3 bytes per pixel (RGB)

        // Napi::Buffer::Copy copies the data.
        return Napi::Buffer<uint8_t>::Copy(env, data, size);
    }
    
    Napi::Value GetDimensions(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (!bitmap_wrapper.ptr) {
            Napi::Error::New(env, "Bitmap has been disposed or was not initialized").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        Napi::Object dimensions = Napi::Object::New(env);
        dimensions.Set("width", Napi::Number::New(env, mtmd_bitmap_get_nx(bitmap_wrapper.ptr.get())));
        dimensions.Set("height", Napi::Number::New(env, mtmd_bitmap_get_ny(bitmap_wrapper.ptr.get())));
        return dimensions;
    }
    
    Napi::Value GetId(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (!bitmap_wrapper.ptr) {
            Napi::Error::New(env, "Bitmap has been disposed or was not initialized").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        const char* id_str = mtmd_bitmap_get_id(bitmap_wrapper.ptr.get());
        if (id_str) {
            return Napi::String::New(env, id_str);
        } else {
            return env.Null();
        }
    }
    
    Napi::Value SetId(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (!bitmap_wrapper.ptr) {
            Napi::Error::New(env, "Bitmap has been disposed or was not initialized").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (info.Length() < 1 || !info[0].IsString()) {
            Napi::TypeError::New(env, "String argument expected for ID").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        std::string id = info[0].As<Napi::String>().Utf8Value();
        mtmd_bitmap_set_id(bitmap_wrapper.ptr.get(), id.c_str());
        
        return env.Undefined();
    }
    
    Napi::Value Dispose(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        // The unique_ptr in bitmap_wrapper will handle freeing the mtmd_bitmap*
        // by calling mtmd_bitmap_free via its deleter.
        // We just need to release the pointer from the unique_ptr.
        bitmap_wrapper.ptr.reset(); 
        return env.Undefined();
    }
};

Napi::FunctionReference MultiBitmap::constructor;

class MultiBitmaps : public Napi::ObjectWrap<MultiBitmaps> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);
        
        Napi::Function func = DefineClass(env, "MultiBitmaps", {
            InstanceMethod("addBitmap", &MultiBitmaps::AddBitmap),
            InstanceMethod("getBitmapCount", &MultiBitmaps::GetBitmapCount),
            InstanceMethod("dispose", &MultiBitmaps::Dispose)
        });
        
        constructor = Napi::Persistent(func); // constructor is Napi::FunctionReference
        constructor.SuppressDestruct();
        
        exports.Set("MultiBitmaps", func);
        return exports;
    }

    // Constructor should be public
    MultiBitmaps(const Napi::CallbackInfo& info) : Napi::ObjectWrap<MultiBitmaps>(info) {
        // bitmaps_collection is already default-initialized (std::vector<mtmd::bitmap>)
    }

    ~MultiBitmaps() {
        // Intentionally empty. Relies on the destructor of bitmaps_collection_cpp_wrapper (mtmd::bitmaps),
        // which will clean up its std::vector<mtmd::bitmap> entries. Each mtmd::bitmap
        // in the vector will have its destructor called, freeing the underlying C mtmd_bitmap*
        // via the mtmd::bitmap_ptr's deleter.
    }
    
    // Made public for access in addonMultimodalTokenize via c_ptr() method
    mtmd::bitmaps bitmaps_collection_cpp_wrapper; // This is the C++ wrapper from mtmd.h

// private: // Keep constructor public, ensure this was intended if previously private
    static Napi::FunctionReference constructor;
    
    Napi::Value AddBitmap(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        
        if (info.Length() < 1 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "MultiBitmap object expected").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        MultiBitmap* wrapped_bitmap = Napi::ObjectWrap<MultiBitmap>::Unwrap(info[0].As<Napi::Object>());
        if (!wrapped_bitmap || !wrapped_bitmap->bitmap_wrapper.ptr) {
            Napi::TypeError::New(env, "Invalid or uninitialized MultiBitmap object").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Create a deep copy of the bitmap data to be owned by this MultiBitmaps collection.
        const mtmd_bitmap* original_c_bitmap = wrapped_bitmap->bitmap_wrapper.ptr.get();
        
        uint32_t nx = mtmd_bitmap_get_nx(original_c_bitmap);
        uint32_t ny = mtmd_bitmap_get_ny(original_c_bitmap);
        const unsigned char* original_data = mtmd_bitmap_get_data(original_c_bitmap);
        
        // mtmd_bitmap_init is expected to copy the data.
        // If it doesn't, we'd need to malloc/memcpy here and pass the new buffer.
        // Assuming mtmd_bitmap_init copies `original_data`.
        mtmd_bitmap* new_c_bitmap = mtmd_bitmap_init(nx, ny, original_data);

        if (!new_c_bitmap) {
            Napi::Error::New(env, "Failed to create a copy of the bitmap for the collection").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        const char* original_id = mtmd_bitmap_get_id(original_c_bitmap);
        if (original_id) {
            mtmd_bitmap_set_id(new_c_bitmap, original_id);
        }
        
        // Add the new C bitmap (now owned by an mtmd::bitmap C++ wrapper) to the collection
        bitmaps_collection_cpp_wrapper.entries.emplace_back(new_c_bitmap); // mtmd::bitmap constructor takes ownership
        
        return env.Undefined();
    }
    
    Napi::Value GetBitmapCount(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        return Napi::Number::New(env, bitmaps_collection_cpp_wrapper.entries.size());
    }
    
    Napi::Value Dispose(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        // Explicitly clear the collection. This also triggers destructors for contained elements.
        bitmaps_collection_cpp_wrapper.entries.clear();
        return env.Undefined();
    }
};

Napi::FunctionReference MultiBitmaps::constructor;

// Implementation of the module functions

Napi::Value addonInitMultimodalBitmapFromBuffer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    // Check arguments - now expecting context and buffer
    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsBuffer()) {
        Napi::TypeError::New(env, "Expected context object and buffer as arguments").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    try {
        // Get the context
        Napi::Object contextObj = info[0].As<Napi::Object>();
        AddonContext* addonCtxInstance = nullptr;
        try {
            addonCtxInstance = Napi::ObjectWrap<AddonContext>::Unwrap(contextObj);
        } catch (const Napi::Error& e) {
            std::string errMsg = "Failed to unwrap AddonContext object for bitmap initialization: ";
            errMsg += e.Message();
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }

        if (!addonCtxInstance) {
            Napi::Error::New(env, "Unwrapped AddonContext instance is null for bitmap initialization.").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        if (!addonCtxInstance->multimodal_ctx) {
            Napi::Error::New(env, "Multimodal context is not initialized. Please ensure the model was loaded with a multimodal projector.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Get the buffer data
        Napi::Buffer<unsigned char> buffer = info[1].As<Napi::Buffer<unsigned char>>();
        unsigned char* buf_data = buffer.Data();
        size_t buf_len = buffer.Length();
        
        // Create MultiBitmap instance
        Napi::Object bitmapObj = MultiBitmap::constructor.New({});
        MultiBitmap* bitmap = Napi::ObjectWrap<MultiBitmap>::Unwrap(bitmapObj);
        
        // Initialize bitmap from buffer using the helper function
        mtmd_bitmap* native_bitmap = mtmd_helper_bitmap_init_from_buf(addonCtxInstance->multimodal_ctx, buf_data, buf_len);
        
        if (!native_bitmap) {
            Napi::Error::New(env, "Failed to initialize bitmap from buffer - mtmd_helper_bitmap_init_from_buf returned null. The image format may not be supported or the buffer may be corrupted.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Set the bitmap pointer in our wrapper
        bitmap->bitmap_wrapper.ptr.reset(native_bitmap);
        
        return bitmapObj;
    } catch (const Napi::Error& e) {
        // Catch NAPI errors thrown from constructor and rethrow
        e.ThrowAsJavaScriptException();
        return env.Undefined();
    } catch (const std::exception& e) {
        // Catch other C++ exceptions
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value addonCreateMultimodalBitmaps(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    try {
        return MultiBitmaps::constructor.New({});
    } catch (const Napi::Error& e) {
        e.ThrowAsJavaScriptException();
        return env.Undefined();
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}



Napi::Value addonMultimodalTokenize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsObject()) {
        Napi::TypeError::New(env, "Expected context object, text string, and MultiBitmaps object").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    try {
        // 1. Get the llama_context pointer
        Napi::Object contextObj = info[0].As<Napi::Object>(); // This is the AddonContext wrapper from JS (context._ctx)
        AddonContext* addonCtxInstance = nullptr;
        try {
            addonCtxInstance = Napi::ObjectWrap<AddonContext>::Unwrap(contextObj);
        } catch (const Napi::Error& e) {
            std::string errMsg = "Failed to unwrap AddonContext object for tokenization: ";
            errMsg += e.Message();
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }

        if (!addonCtxInstance) {
            Napi::Error::New(env, "Unwrapped AddonContext instance is null for tokenization.").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        if (addonCtxInstance->disposed) {
            Napi::Error::New(env, "Llama context has been disposed (for tokenization).").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        llama_context* llama_ctx = addonCtxInstance->ctx; // Access the ctx member
        if (!llama_ctx) {
            Napi::Error::New(env, "Invalid llama_context pointer (ctx member is null) in AddonContext instance for tokenization.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Access mctx using the C API:
        // 1. Get the C struct llama_model* from the llama_context*
        // const struct llama_model * l_model = llama_get_model(llama_ctx); // This line is not strictly needed anymore for mctx
        // if (!l_model) { // We still need llama_ctx, so l_model might be useful for other things or checks
        //     Napi::Error::New(env, "Failed to get model from llama_context. The model pointer is null.").ThrowAsJavaScriptException();
        //     return env.Undefined();
        // }
        
        // 2. Access the mctx member from the C struct llama_model (REMOVED)
        // This assumes that 'mctx' is a publicly accessible member of 'struct llama_model'
        // when compiled with multimodal support, as populated during model loading.
        // mtmd_context* mctx = l_model->mctx; (OLD WAY - CAUSES ERROR)

        // Use the per-context multimodal context
        mtmd_context* mctx = addonCtxInstance->multimodal_ctx;

        if (!mctx) {
            Napi::Error::New(env, "Multimodal context (mctx) is null in the context. Ensure the loaded model supports multimodal capabilities and was initialized correctly.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 2. Get the text prompt
        std::string promptText = info[1].As<Napi::String>().Utf8Value();
        
        // 3. Get the MultiBitmaps NAPI wrapper
        MultiBitmaps* multi_bitmaps_napi = Napi::ObjectWrap<MultiBitmaps>::Unwrap(info[2].As<Napi::Object>());
        if (!multi_bitmaps_napi) { // Corrected typo: multi_bit_maps_napi -> multi_bitmaps_napi
            Napi::Error::New(env, "Invalid MultiBitmaps object").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 4. Prepare the input text struct for mtmd_tokenize
        mtmd_input_text inputText = { // This should be in scope
            promptText.c_str(),
            true, // add_special
            true  // parse_special
        };
        
        // 5. Initialize mtmd::input_chunks (C++ wrapper) for tokenization result
        // The C++ wrapper mtmd::input_chunks will manage the underlying mtmd_input_chunks*
        mtmd::input_chunks chunks_cpp_wrapper(mtmd_input_chunks_init());
        if (!chunks_cpp_wrapper.ptr) {
            Napi::Error::New(env, "Failed to initialize input chunks for tokenization").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 6. Get C-style array of const mtmd_bitmap* pointers using the helper from mtmd::bitmaps
        // Corrected typo: multi_bit_maps_napi -> multi_bit_maps_napi
        std::vector<const mtmd_bitmap*> c_bitmap_pointers = multi_bitmaps_napi->bitmaps_collection_cpp_wrapper.c_ptr();
        
        // 7. Call mtmd_tokenize
        int32_t tokenizeResult = mtmd_tokenize(
            addonCtxInstance->multimodal_ctx,
            chunks_cpp_wrapper.ptr.get(), // Get the raw C pointer from the C++ wrapper
            &inputText, // inputText should be in scope
            c_bitmap_pointers.data(),   // Data pointer of the vector of const mtmd_bitmap*
            c_bitmap_pointers.size()    // Number of bitmaps
        );
        
        if (tokenizeResult != 0) {
            // mtmd_tokenize returns specific error codes (1 or 2) or 0 on success.
            std::string error_msg = "Failed to tokenize multimodal input. Error code: " + std::to_string(tokenizeResult);
            Napi::Error::New(env, error_msg).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 8. Convert tokenized result (chunks) into a JS object
        Napi::Object resultObj = Napi::Object::New(env);
        Napi::Array jsChunksArray = Napi::Array::New(env);
        
        size_t num_chunks = mtmd_input_chunks_size(chunks_cpp_wrapper.ptr.get()); // Use C API with raw pointer
        // Or use the C++ wrapper: size_t num_chunks = chunks_cpp_wrapper.size();

        for (size_t i = 0; i < num_chunks; ++i) {
            // const mtmd_input_chunk* c_chunk = mtmd_input_chunks_get(chunks_cpp_wrapper.ptr.get(), i); // C API
            const mtmd_input_chunk* c_chunk = chunks_cpp_wrapper[i]; // C++ wrapper operator[]

            if (!c_chunk) continue; // Should not happen if num_chunks is correct

            Napi::Object jsChunkObj = Napi::Object::New(env);
            enum mtmd_input_chunk_type chunk_type = mtmd_input_chunk_get_type(c_chunk);
            jsChunkObj.Set("type", Napi::Number::New(env, static_cast<int>(chunk_type)));

            if (chunk_type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
                size_t n_text_tokens = 0;
                const llama_token* text_tokens = mtmd_input_chunk_get_tokens_text(c_chunk, &n_text_tokens);
                Napi::Array jsTextTokens = Napi::Array::New(env, n_text_tokens);
                for (size_t j = 0; j < n_text_tokens; ++j) {
                    jsTextTokens[j] = Napi::Number::New(env, text_tokens[j]);
                }
                jsChunkObj.Set("tokens", jsTextTokens);
            } else if (chunk_type == MTMD_INPUT_CHUNK_TYPE_IMAGE) { // Corrected enum name
                const mtmd_image_tokens* image_tokens_info = mtmd_input_chunk_get_tokens_image(c_chunk);
                if (image_tokens_info) {
                    Napi::Object jsImageInfo = Napi::Object::New(env);
                    jsImageInfo.Set("tokenCount", Napi::Number::New(env, mtmd_image_tokens_get_n_tokens(image_tokens_info)));
                    jsImageInfo.Set("nx", Napi::Number::New(env, mtmd_image_tokens_get_nx(image_tokens_info)));
                    jsImageInfo.Set("ny", Napi::Number::New(env, mtmd_image_tokens_get_ny(image_tokens_info)));
                    const char* img_id = mtmd_image_tokens_get_id(image_tokens_info);
                    if (img_id) {
                        jsImageInfo.Set("id", Napi::String::New(env, img_id));
                    } else {
                        jsImageInfo.Set("id", env.Null());
                    }
                    jsImageInfo.Set("nPos", Napi::Number::New(env, mtmd_image_tokens_get_n_pos(image_tokens_info)));
                    // Note: The actual token IDs for images are not directly exposed here by mtmd.h.
                    // This part of the API returns metadata about the image token block.
                    jsChunkObj.Set("imageInfo", jsImageInfo);
                }
            }
            jsChunksArray[i] = jsChunkObj;
        }
        
        resultObj.Set("chunks", jsChunksArray);
        
        // mtmd::input_chunks destructor will call mtmd_input_chunks_free on chunks_cpp_wrapper.ptr
        return resultObj;

    } catch (const Napi::Error& e) {
        // Catch NAPI errors thrown previously and rethrow
        e.ThrowAsJavaScriptException();
        return env.Undefined();
    } catch (const std::exception& e) {
        // Catch other C++ exceptions (e.g., std::bad_alloc, std::runtime_error)
        Napi::Error::New(env, std::string("C++ exception in addonMultimodalTokenize: ") + e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

/**
 * Evaluate multimodal chunks using mtmd_helper_eval_chunks()
 * This properly handles both text tokens and image embeddings
 */
Napi::Value addonMultimodalEvaluateChunks(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsObject()) {
        Napi::TypeError::New(env, "Expected context object and tokenize result object").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    try {
        // 1. Get the llama_context pointer
        Napi::Object contextObj = info[0].As<Napi::Object>();
        AddonContext* addonCtxInstance = nullptr;
        try {
            addonCtxInstance = Napi::ObjectWrap<AddonContext>::Unwrap(contextObj);
        } catch (const Napi::Error& e) {
            std::string errMsg = "Failed to unwrap AddonContext object for chunk evaluation: ";
            errMsg += e.what();
            Napi::TypeError::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (!addonCtxInstance) {
            Napi::TypeError::New(env, "Unwrapped AddonContext instance is null for chunk evaluation.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (!addonCtxInstance->ctx) {
            Napi::TypeError::New(env, "Llama context has been disposed (for chunk evaluation).").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        llama_context* lctx = addonCtxInstance->ctx;
        if (!lctx) {
            Napi::TypeError::New(env, "Invalid llama_context pointer (ctx member is null) in AddonContext instance for chunk evaluation.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Use the per-context multimodal context instead of global
        if (!addonCtxInstance->multimodal_ctx) {
            Napi::TypeError::New(env, "Multimodal context (mctx) is null in the context. Ensure the loaded model supports multimodal capabilities and was initialized correctly.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 3. Get the tokenize result object containing chunks
        Napi::Object tokenizeResult = info[1].As<Napi::Object>();
        if (!tokenizeResult.Has("chunks")) {
            Napi::TypeError::New(env, "Tokenize result must have 'chunks' property").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        Napi::Array chunksArray = tokenizeResult.Get("chunks").As<Napi::Array>();
        size_t numChunks = chunksArray.Length();
        
        // 4. Reconstruct mtmd_input_chunks from the JavaScript object
        // Note: This is a simplification. In a production implementation, you might want to
        // cache the mtmd_input_chunks from the tokenize call to avoid reconstruction.
        mtmd_input_chunks* chunks = mtmd_input_chunks_init();
        if (!chunks) {
            Napi::Error::New(env, "Failed to initialize input chunks for evaluation").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // For now, we'll use a simplified approach where we re-tokenize to get the chunks
        // This is not ideal but necessary given the current API design
        Napi::Error::New(env, "Chunk evaluation requires re-tokenization. Please use the new evaluateMultimodalTokenizeAndEvaluate function instead.").ThrowAsJavaScriptException();
        return env.Undefined();
        
    } catch (const Napi::Error& e) {
        e.ThrowAsJavaScriptException();
        return env.Undefined();
    } catch (const std::exception& e) {
        Napi::Error::New(env, std::string("C++ exception in addonMultimodalEvaluateChunks: ") + e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

/**
 * Combined tokenize and evaluate function that properly handles multimodal input
 * This function tokenizes the input and immediately evaluates the chunks using mtmd_helper_eval_chunks()
 */
Napi::Value addonMultimodalTokenizeAndEvaluate(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsString() || !info[2].IsObject()) {
        Napi::TypeError::New(env, "Expected context object, text string, and MultiBitmaps object").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    try {
        // 1. Get the llama_context pointer
        Napi::Object contextObj = info[0].As<Napi::Object>();
        AddonContext* addonCtxInstance = nullptr;
        try {
            addonCtxInstance = Napi::ObjectWrap<AddonContext>::Unwrap(contextObj);
        } catch (const Napi::Error& e) {
            std::string errMsg = "Failed to unwrap AddonContext object for tokenize and evaluate: ";
            errMsg += e.what();
            Napi::TypeError::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (!addonCtxInstance) {
            Napi::TypeError::New(env, "Unwrapped AddonContext instance is null for tokenize and evaluate.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (!addonCtxInstance->ctx) {
            Napi::TypeError::New(env, "Llama context has been disposed (for tokenize and evaluate).").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        llama_context* lctx = addonCtxInstance->ctx;
        if (!lctx) {
            Napi::TypeError::New(env, "Invalid llama_context pointer (ctx member is null) in AddonContext instance for tokenize and evaluate.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Use the per-context multimodal context instead of global
        if (!addonCtxInstance->multimodal_ctx) {
            Napi::TypeError::New(env, "Multimodal context (mctx) is null in the context. Ensure the loaded model supports multimodal capabilities and was initialized correctly.").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 3. Get text and MultiBitmaps
        std::string text = info[1].As<Napi::String>().Utf8Value();
        Napi::Object bitmapsObj = info[2].As<Napi::Object>();
        
        MultiBitmaps* bitmaps_instance = nullptr;
        try {
            bitmaps_instance = Napi::ObjectWrap<MultiBitmaps>::Unwrap(bitmapsObj);
        } catch (const Napi::Error& e) {
            Napi::TypeError::New(env, "Invalid MultiBitmaps object").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        if (!bitmaps_instance) {
            Napi::TypeError::New(env, "MultiBitmaps instance is null").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 4. Prepare input for tokenization
        mtmd_input_text input_text;
        input_text.text = text.c_str();
        input_text.add_special = true;
        input_text.parse_special = true;
        
        // 5. Tokenize the input
        mtmd_input_chunks* chunks = mtmd_input_chunks_init();
        if (!chunks) {
            Napi::Error::New(env, "Failed to initialize input chunks for tokenize and evaluate").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // Get C-style array of const mtmd_bitmap* pointers using the helper from mtmd::bitmaps
        std::vector<const mtmd_bitmap*> c_bitmap_pointers = bitmaps_instance->bitmaps_collection_cpp_wrapper.c_ptr();
        
        int32_t tokenize_result = mtmd_tokenize(addonCtxInstance->multimodal_ctx, chunks, &input_text, 
                                               c_bitmap_pointers.data(), 
                                               c_bitmap_pointers.size());
        
        if (tokenize_result != 0) {
            mtmd_input_chunks_free(chunks);
            std::string errMsg = "Failed to tokenize multimodal input. Error code: " + std::to_string(tokenize_result);
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 6. Evaluate the chunks using mtmd_helper_eval_chunks()
        llama_pos n_past = addonCtxInstance->n_cur; // Use current sequence position
        llama_pos new_n_past = 0;
        
        int32_t eval_result = mtmd_helper_eval_chunks(addonCtxInstance->multimodal_ctx, lctx, chunks, 
                                                     n_past, 0, // seq_id = 0
                                                     addonCtxInstance->context_params.n_batch, // n_batch
                                                     true, // logits_last
                                                     &new_n_past);
        
        if (eval_result != 0) {
            mtmd_input_chunks_free(chunks);
            std::string errMsg = "Failed to evaluate multimodal chunks. Error code: " + std::to_string(eval_result);
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
        // 7. Update the context's sequence position to maintain sync with native state
        addonCtxInstance->n_cur = new_n_past;
        
        // 8. Clean up and return success
        mtmd_input_chunks_free(chunks);
        
        // Return result object with detailed position information for JavaScript layer sync
        Napi::Object resultObj = Napi::Object::New(env);
        resultObj.Set("success", Napi::Boolean::New(env, true));
        resultObj.Set("tokensProcessed", Napi::Number::New(env, new_n_past - n_past));
        resultObj.Set("newSequenceLength", Napi::Number::New(env, new_n_past));
        resultObj.Set("previousSequenceLength", Napi::Number::New(env, n_past));
        
        return resultObj;
        
    } catch (const Napi::Error& e) {
        e.ThrowAsJavaScriptException();
        return env.Undefined();
    } catch (const std::exception& e) {
        Napi::Error::New(env, std::string("C++ exception in addonMultimodalTokenizeAndEvaluate: ") + e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}


Napi::Object InitMultimodal(Napi::Env env, Napi::Object exports) {
    MultiBitmap::Init(env, exports);
    MultiBitmaps::Init(env, exports);
    
    exports.Set("initMultimodalBitmapFromBuffer", 
                Napi::Function::New(env, addonInitMultimodalBitmapFromBuffer, "initMultimodalBitmapFromBuffer"));
    exports.Set("createMultimodalBitmaps", 
                Napi::Function::New(env, addonCreateMultimodalBitmaps, "createMultimodalBitmaps"));
    exports.Set("multimodalTokenize", 
                Napi::Function::New(env, addonMultimodalTokenize, "multimodalTokenize"));
    exports.Set("multimodalEvaluateChunks", 
                Napi::Function::New(env, addonMultimodalEvaluateChunks, "multimodalEvaluateChunks"));
    exports.Set("multimodalTokenizeAndEvaluate", 
                Napi::Function::New(env, addonMultimodalTokenizeAndEvaluate, "multimodalTokenizeAndEvaluate"));
                
    return exports;
}
