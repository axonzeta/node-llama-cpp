import {BindingModule} from "../AddonTypes.js";
import {MultiBitmap, MultiBitmaps, MultimodalTokenizeResult, VisionEncoderState} from "./types.js";
import {Token} from "../../types.js";
import type {LlamaContext} from "../../evaluator/LlamaContext/LlamaContext.js"; // Corrected import path

/**
 * Creates a hash for media buffer (image or audio) using FNV algorithm
 * (same algorithm as used in llama.cpp server)
 * 
 * @param buffer The media data buffer
 * @returns Hash string
 */
export function createMediaHash(buffer: Buffer | Float32Array): string {
    const fnvPrime = 0x01000193;
    const fnvOffset = 0x811c9dc5;
    
    let hash = fnvOffset;
    
    if (buffer instanceof Float32Array) {
        // For Float32Array, hash the underlying ArrayBuffer
        const uint8View = new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        for (let i = 0; i < uint8View.length; i++) {
            hash = Math.imul(hash ^ (uint8View[i] || 0), fnvPrime);
        }
    } else {
        // For Buffer
        for (let i = 0; i < buffer.length; i++) {
            hash = Math.imul(hash ^ (buffer[i] || 0), fnvPrime);
        }
    }
    
    return (hash >>> 0).toString(); // Convert to unsigned 32-bit integer and then to string
}

/**
 * Creates a hash for an image buffer using FNV algorithm
 * (same algorithm as used in llama.cpp server)
 * 
 * @param buffer The image data buffer
 * @returns Hash string
 * @deprecated Use createMediaHash instead
 */
export function createImageHash(buffer: Buffer): string {
    return createMediaHash(buffer);
}

/**
 * Manager for multimodal image and audio processing
 */
export class MultimodalManager {
    private readonly _bindings: BindingModule;
    
    /**
     * Create a new MultimodalManager
     * 
     * @param bindings The llama.cpp bindings
     */
    constructor(bindings: BindingModule) {
        this._bindings = bindings;
    }
    
    /**
     * Check if the current model supports audio input
     * 
     * @param context The LlamaContext to check
     * @returns true if audio is supported, false otherwise
     */
    public supportsAudio(context: LlamaContext): boolean {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            return false;
        }
        
        if (typeof this._bindings.multimodalSupportsAudio !== 'function') {
            return false;
        }
        
        return this._bindings.multimodalSupportsAudio(nativeContext);
    }
    
    /**
     * Get the audio bitrate (sample rate) expected by the model
     * 
     * @param context The LlamaContext to check
     * @returns Audio bitrate in Hz (e.g., 16000 for Whisper), or -1 if audio is not supported
     */
    public getAudioBitrate(context: LlamaContext): number {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            return -1;
        }
        
        if (typeof this._bindings.multimodalGetAudioBitrate !== 'function') {
            return -1;
        }
        
        return this._bindings.multimodalGetAudioBitrate(nativeContext);
    }
    
    /**
     * Load an image from a buffer
     * 
     * @param context The LlamaContext to use for multimodal processing
     * @param imageBuffer The image buffer data
     * @returns MultiBitmap instance
     */
    public loadImageFromBuffer(context: LlamaContext, imageBuffer: Buffer): MultiBitmap {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot load image from buffer.");
        }
        
        const bitmap = this._bindings.initMultimodalBitmapFromBuffer(nativeContext, imageBuffer);
        
        // Automatically generate and set a hash ID for the bitmap
        const hash = createMediaHash(imageBuffer);
        bitmap.setId(hash.toString());
        
        return bitmap;
    }
    
    /**
     * Load audio from a Float32Array buffer
     * 
     * @param context The LlamaContext to use for multimodal processing
     * @param audioBuffer The audio buffer data (Float32 PCM samples)
     * @returns MultiBitmap instance representing audio
     */
    public loadAudioFromBuffer(context: LlamaContext, audioBuffer: Float32Array): MultiBitmap {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot load audio from buffer.");
        }
        
        if (!this.supportsAudio(context)) {
            throw new Error("Audio input is not supported by this model.");
        }
        
        if (typeof this._bindings.initMultimodalBitmapFromAudio !== 'function') {
            throw new Error("initMultimodalBitmapFromAudio is not supported by the current bindings. Ensure the native addon is compiled with audio support and is up to date.");
        }
        
        const bitmap = this._bindings.initMultimodalBitmapFromAudio(nativeContext, audioBuffer);
        
        // Automatically generate and set a hash ID for the bitmap
        const hash = createMediaHash(audioBuffer);
        bitmap.setId(hash.toString());
        
        return bitmap;
    }
    
    /**
     * Load media (image or audio) from a buffer
     * 
     * @param context The LlamaContext to use for multimodal processing
     * @param mediaBuffer The media buffer data (Buffer for images/audio files, Float32Array for raw PCM audio)
     * @returns MultiBitmap instance
     */
    public loadMediaFromBuffer(context: LlamaContext, mediaBuffer: Buffer | Float32Array): MultiBitmap {
        if (mediaBuffer instanceof Float32Array) {
            // Raw PCM audio data
            return this.loadAudioFromBuffer(context, mediaBuffer);
        } else {
            // Buffer data - could be image or audio file format (MP3, WAV, etc.)
            const nativeContext = context._ctx;
            if (!nativeContext) {
                throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot load media from buffer.");
            }
            
            // Create the bitmap using initMultimodalBitmapFromBuffer
            const bitmap = this._bindings.initMultimodalBitmapFromBuffer(nativeContext, mediaBuffer);
            
            // Check if the bitmap was detected as audio
            // If it's audio, we need to verify the model supports audio
            if (bitmap.isAudio()) {
                console.log("Audio file detected in buffer format.");
                
                // Make sure audio is supported by the model
                if (!this.supportsAudio(context)) {
                    bitmap.dispose();
                    throw new Error("Audio input was detected but this model doesn't support audio processing.");
                }
                
                // Get expected audio bitrate for this model
                const bitrate = this.getAudioBitrate(context);
                console.log(`Audio detected - model expects ${bitrate}Hz audio.`);
                
                try {
                    // Get and log audio dimensions for debugging
                    const dimensions = bitmap.getDimensions();
                    console.log("Audio dimensions:", dimensions);
                    
                    // No need for extra processing - the MTMD library handles MP3 files directly
                    console.log("Native audio processing is being used through MTMD library.");
                } catch (error: any) {
                    bitmap.dispose();
                    throw new Error(`Audio processing error: ${error.message || error}`);
                }
            }
            
            // Automatically generate and set a hash ID for the bitmap
            const hash = createMediaHash(mediaBuffer);
            bitmap.setId(hash.toString());
            
            return bitmap;
        }
    }
    
    /**
     * Create a new collection of bitmaps
     * 
     * @returns MultiBitmaps collection
     */
    public createBitmapCollection(): MultiBitmaps {
        return this._bindings.createMultimodalBitmaps();
    }

    /**
     * Add an image to a context for multimodal processing
     * 
     * @param context The LlamaContext to use
     * @param text The text prompt to process
     * @param images Array of image buffers to include
     * @returns The tokenized result
     */
        public tokenize(context: LlamaContext, text: string, images: Buffer[]): MultimodalTokenizeResult {
        // Extract the native context from the LlamaContext object
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot tokenize multimodal input.");
        }
        
        // Create bitmap collection
        const bitmaps = this.createBitmapCollection();
        
        try {
            // Array to keep track of bitmaps for proper cleanup
            const createdBitmaps: MultiBitmap[] = [];
            
            // Load each image and add to collection
            for (const imageBuffer of images) {
                const bitmap = this.loadImageFromBuffer(context, imageBuffer);
                bitmaps.addBitmap(bitmap);
                createdBitmaps.push(bitmap);
            }
            
            // Tokenize the text with images using the native context
            const result = this._bindings.multimodalTokenize(nativeContext, text, bitmaps);
            
            // Clean up the bitmap resources
            for (const bitmap of createdBitmaps) {
                bitmap.dispose();
            }
            bitmaps.dispose();
            
            return result;
        } catch (error) {
            // Make sure to clean up resources even if tokenization fails
            bitmaps.dispose();
            throw error;
        }
    }

    /**
     * Evaluate multimodal chunks using the proper mtmd_helper_eval_chunks() approach
     * 
     * @param context The LlamaContext to use
     * @param tokenizeResult The result from multimodalTokenize()
     * @returns Evaluation result with success status and metadata
     */
    public evaluateMultimodalChunks(context: LlamaContext, tokenizeResult: MultimodalTokenizeResult): object {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot evaluate multimodal chunks.");
        }

        if (typeof this._bindings.multimodalEvaluateChunks !== 'function') {
            throw new Error("multimodalEvaluateChunks is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        return this._bindings.multimodalEvaluateChunks(nativeContext, tokenizeResult);
    }    /**
     * Combined tokenize and evaluate function that properly handles multimodal input
     * This is the recommended approach for multimodal processing
     * 
     * @param context The LlamaContext to use
     * @param text The text prompt to process
     * @param images Array of image buffers to include
     * @returns Evaluation result with success status and metadata
     */
    public tokenizeAndEvaluate(context: LlamaContext, text: string, images: Buffer[]): object {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot tokenize and evaluate multimodal input.");
        }

        if (typeof this._bindings.multimodalTokenizeAndEvaluate !== 'function') {
            throw new Error("multimodalTokenizeAndEvaluate is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        // Create bitmap collection
        const bitmaps = this.createBitmapCollection();

        try {
            // Array to keep track of bitmaps for proper cleanup
            const createdBitmaps: MultiBitmap[] = [];
            
            // Load each image and add to collection
            for (const imageBuffer of images) {
                const bitmap = this.loadImageFromBuffer(context, imageBuffer);
                createdBitmaps.push(bitmap);
                bitmaps.addBitmap(bitmap);
            }

            // Call the combined tokenize and evaluate function
            const result = this._bindings.multimodalTokenizeAndEvaluate(nativeContext, text, bitmaps);
            
            // Record the processed tokens for sequence inheritance
            if (result && typeof result === 'object' && 'tokensProcessed' in result) {
                const tokensProcessed = (result as any).tokensProcessed;
                if (typeof tokensProcessed === 'number' && tokensProcessed > 0) {
                    // Create placeholder tokens to represent the multimodal content
                    // We use negative values to distinguish them from regular tokens
                    const placeholderTokens: Token[] = [];
                    for (let i = 0; i < tokensProcessed; i++) {
                        // Create unique placeholder token IDs for multimodal content
                        // Using negative values starting from -1000 to avoid conflicts
                        placeholderTokens.push((-1000 - i) as Token);
                    }
                    
                    // Record these tokens so sequences can inherit them
                    context._recordContextLevelTokens(placeholderTokens);
                }
            }
            
            // Clean up individual bitmaps (collection is disposed below)
            createdBitmaps.forEach(bitmap => bitmap.dispose());
            
            return result;
        } catch (error) {
            // Make sure to clean up resources even if processing fails
            bitmaps.dispose();
            throw error;
        }
    }
    
    /**
     * Combined tokenize and evaluate function that handles mixed media (images and audio)
     * This is the recommended approach for multimodal processing with mixed media types
     * 
     * @param context The LlamaContext to use
     * @param text The text prompt to process
     * @param media Array of media buffers (Buffer for images/audio files, Float32Array for raw PCM audio)
     * @returns Evaluation result with success status and metadata
     */
    public tokenizeAndEvaluateMedia(context: LlamaContext, text: string, media: Array<Buffer | Float32Array>): object {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot tokenize and evaluate multimodal input.");
        }

        if (typeof this._bindings.multimodalTokenizeAndEvaluate !== 'function') {
            throw new Error("multimodalTokenizeAndEvaluate is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        // Create bitmap collection
        const bitmaps = this.createBitmapCollection();

        try {
            // Array to keep track of bitmaps for proper cleanup
            const createdBitmaps: MultiBitmap[] = [];
            
            // Detect if we have any audio content - this will be handled specially
            const hasAudioContent = media.some(m => {
                if (m instanceof Float32Array) return true; // Raw PCM audio
                // For buffer media, we'd need to check after loading
                return false;
            });
            
            // Process each media item based on its type
            for (const mediaBuffer of media) {
                try {
                    // Load the media buffer using the appropriate method
                    const bitmap = this.loadMediaFromBuffer(context, mediaBuffer);
                    
                    if (bitmap.isAudio()) {
                        // For audio content, ensure the model supports it
                        if (!this.supportsAudio(context)) {
                            bitmap.dispose(); // Clean up this bitmap
                            throw new Error("Audio input was detected but this model doesn't support audio processing.");
                        }
                        console.log("Processing audio input in tokenizeAndEvaluateMedia...");
                    }
                    
                    createdBitmaps.push(bitmap);
                    bitmaps.addBitmap(bitmap);
                } catch (error: any) {
                    // Clean up any bitmaps that were already created
                    createdBitmaps.forEach(b => b.dispose());
                    throw new Error(`Failed to process media: ${error.message || error}`);
                }
            }

            // For audio content, we need special handling to avoid GGML errors
            const anyAudioBitmap = createdBitmaps.some(bitmap => bitmap.isAudio());
            
            if (anyAudioBitmap) {
                console.log("Audio content detected, ensuring proper handling...");
            }

            // Call the combined tokenize and evaluate function with proper media type handling
            let result;
            try {
                // For audio processing, wrap in try/catch for better error messages
                result = this._bindings.multimodalTokenizeAndEvaluate(nativeContext, text, bitmaps);
            } catch (error: any) {
                let errorMsg = `Failed to tokenize and evaluate media: ${error.message || error}`;
                
                // Check if this is likely a GGML assertion error related to audio processing
                if (error.message && error.message.includes("GGML_ASSERT") && error.message.includes("(!is_2D || OH > 0)")) {
                    errorMsg += "\n\nThis is a GGML matrix dimension error that occurs when processing audio.";
                    errorMsg += "\nPossible solutions:";
                    errorMsg += "\n1. Make sure you're using a model that fully supports audio input";
                    errorMsg += "\n2. Try a different audio file";
                    errorMsg += "\n3. Verify the model was loaded with the correct projector file";
                    errorMsg += "\n4. Check if your audio format is compatible with this model";
                } else if (error.message && error.message.includes("GGML_ASSERT")) {
                    errorMsg += "\n\nThis appears to be a GGML matrix dimension error that can happen with audio processing. " +
                                "Please ensure you're using a model with proper audio support.";
                }
                
                throw new Error(errorMsg);
            }
            
            // Record the processed tokens for sequence inheritance
            if (result && typeof result === 'object' && 'tokensProcessed' in result) {
                const tokensProcessed = (result as any).tokensProcessed;
                if (typeof tokensProcessed === 'number' && tokensProcessed > 0) {
                    // Create placeholder tokens to represent the multimodal content
                    // We use negative values to distinguish them from regular tokens
                    const placeholderTokens: Token[] = [];
                    for (let i = 0; i < tokensProcessed; i++) {
                        // Create unique placeholder token IDs for multimodal content
                        // Using negative values starting from -1000 to avoid conflicts
                        placeholderTokens.push((-1000 - i) as Token);
                    }
                    
                    // Record these tokens so sequences can inherit them
                    context._recordContextLevelTokens(placeholderTokens);
                }
            }
            
            // Clean up individual bitmaps (collection is disposed below)
            createdBitmaps.forEach(bitmap => bitmap.dispose());
            
            return result;
        } catch (error) {
            // Make sure to clean up resources even if processing fails
            bitmaps.dispose();
            throw error;
        }
    }
    
    /**
     * Extract tokens from multimodal input as a flat array
     * This method tokenizes multimodal input and returns all tokens (text, image, audio) 
     * as a single flat array that can be used in subsequent evaluation calls.
     * 
     * @param context The LlamaContext to use
     * @param text The text prompt to process
     * @param media Array of media buffers (Buffer for images/audio files, Float32Array for raw PCM audio)
     * @returns Array of token IDs
     */
    public async getTokens(context: LlamaContext, text: string, media: Array<Buffer | Float32Array>): Promise<number[]> {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot extract tokens from multimodal input.");
        }

        if (typeof this._bindings.multimodalGetTokens !== 'function') {
            throw new Error("multimodalGetTokens is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        // Create bitmap collection
        const bitmaps = this.createBitmapCollection();

        try {
            // Array to keep track of bitmaps for proper cleanup
            const createdBitmaps: MultiBitmap[] = [];
            
            // Load each media item and add to collection
            for (const mediaBuffer of media) {
                try {
                    const bitmap = this.loadMediaFromBuffer(context, mediaBuffer);
                    
                    if (bitmap.isAudio()) {
                        // For audio content, ensure the model supports it
                        if (!this.supportsAudio(context)) {
                            bitmap.dispose(); // Clean up this bitmap
                            throw new Error("Audio input was detected but this model doesn't support audio processing.");
                        }
                    }
                    
                    createdBitmaps.push(bitmap);
                    bitmaps.addBitmap(bitmap);
                } catch (error: any) {
                    // Clean up any bitmaps that were already created
                    createdBitmaps.forEach(b => b.dispose());
                    throw new Error(`Failed to process media for token extraction: ${error.message || error}`);
                }
            }

            // Extract tokens using the native function
            const tokens = this._bindings.multimodalGetTokens(nativeContext, text, bitmaps);
            
            // Clean up individual bitmaps
            createdBitmaps.forEach(bitmap => bitmap.dispose());
            
            return tokens;
        } catch (error) {
            // Make sure to clean up resources even if token extraction fails
            bitmaps.dispose();
            throw error;
        }
    }

    /**
     * Tokenize multimodal content and return both chunks and flat token array
     * This is an enhanced version of the tokenize method that includes both the original
     * chunk structure and a new flat token array.
     * 
     * @param context The LlamaContext to use
     * @param text The text prompt to process
     * @param media Array of media buffers (Buffer for images/audio files, Float32Array for raw PCM audio)
     * @returns Tokenization result with both chunks and flat tokens
     */
    public async tokenizeMedia(context: LlamaContext, text: string, media: Array<Buffer | Float32Array>): Promise<MultimodalTokenizeResult> {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot tokenize multimodal input.");
        }

        // Create bitmap collection
        const bitmaps = this.createBitmapCollection();

        try {
            // Array to keep track of bitmaps for proper cleanup
            const createdBitmaps: MultiBitmap[] = [];
            
            // Load each media item and add to collection
            for (const mediaBuffer of media) {
                try {
                    const bitmap = this.loadMediaFromBuffer(context, mediaBuffer);
                    
                    if (bitmap.isAudio()) {
                        // For audio content, ensure the model supports it
                        if (!this.supportsAudio(context)) {
                            bitmap.dispose(); // Clean up this bitmap
                            throw new Error("Audio input was detected but this model doesn't support audio processing.");
                        }
                    }
                    
                    createdBitmaps.push(bitmap);
                    bitmaps.addBitmap(bitmap);
                } catch (error: any) {
                    // Clean up any bitmaps that were already created
                    createdBitmaps.forEach(b => b.dispose());
                    throw new Error(`Failed to process media for tokenization: ${error.message || error}`);
                }
            }

            // Tokenize using the native function (enhanced version includes tokens field)
            const result = this._bindings.multimodalTokenize(nativeContext, text, bitmaps);
            
            // Clean up individual bitmaps
            createdBitmaps.forEach(bitmap => bitmap.dispose());
            
            return result;
        } catch (error) {
            // Make sure to clean up resources even if tokenization fails
            bitmaps.dispose();
            throw error;
        }
    }
    
    /**
     * Extract vision encoder state from a processed image for caching
     * @param context The LlamaContext to use
     * @param bitmap The MultiBitmap containing the processed image
     * @returns Vision encoder state object for caching
     */
    public extractVisionState(context: LlamaContext, bitmap: MultiBitmap): VisionEncoderState {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot extract vision state.");
        }

        if (typeof this._bindings.multimodalExtractVisionState !== 'function') {
            throw new Error("multimodalExtractVisionState is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        return this._bindings.multimodalExtractVisionState(nativeContext, bitmap);
    }
    
    /**
     * Load and apply vision encoder state from cached data
     * @param context The LlamaContext to use
     * @param visionState The cached vision encoder state
     * @returns Recreated MultiBitmap with vision encoder state applied
     */
    public loadVisionState(context: LlamaContext, visionState: VisionEncoderState): MultiBitmap {
        const nativeContext = context._ctx;
        if (!nativeContext) {
            throw new Error("Invalid LlamaContext object: _ctx is missing. Cannot load vision state.");
        }

        if (typeof this._bindings.multimodalLoadVisionState !== 'function') {
            throw new Error("multimodalLoadVisionState is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        return this._bindings.multimodalLoadVisionState(nativeContext, visionState);
    }
    
    /**
     * Serialize vision encoder state to a binary buffer for efficient storage
     * @param visionState The vision encoder state to serialize
     * @returns Binary buffer containing serialized state
     */
    public serializeVisionState(visionState: VisionEncoderState): Buffer {
        if (typeof this._bindings.multimodalSerializeVisionState !== 'function') {
            throw new Error("multimodalSerializeVisionState is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        return this._bindings.multimodalSerializeVisionState(visionState);
    }
    
    /**
     * Deserialize vision encoder state from a binary buffer
     * @param buffer Binary buffer containing serialized vision state
     * @returns Deserialized vision encoder state object
     */
    public deserializeVisionState(buffer: Buffer): VisionEncoderState {
        if (typeof this._bindings.multimodalDeserializeVisionState !== 'function') {
            throw new Error("multimodalDeserializeVisionState is not supported by the current bindings. Ensure the native addon is compiled with multimodal support and is up to date.");
        }

        return this._bindings.multimodalDeserializeVisionState(buffer);
    }
    
    /**
     * Release resources
     */
    public dispose(): void {
        // Multimodal context is now automatically managed per-context
        // No manual cleanup required - resources are automatically released
        // when the context is disposed
    }
}
