import {BindingModule} from "../AddonTypes.js";
import {MultiBitmap, MultiBitmaps, MultimodalTokenizeResult} from "./types.js";
import {Token} from "../../types.js";
import type {LlamaContext} from "../../evaluator/LlamaContext/LlamaContext.js"; // Corrected import path

/**
 * Creates a hash for an image buffer using FNV algorithm
 * (same algorithm as used in llama.cpp server)
 * 
 * @param buffer The image data buffer
 * @returns Hash string
 */
export function createImageHash(buffer: Buffer): string {
    const fnvPrime = 0x01000193;
    const fnvOffset = 0x811c9dc5;
    
    let hash = fnvOffset;
    for (let i = 0; i < buffer.length; i++) {
        hash = Math.imul(hash ^ (buffer[i] || 0), fnvPrime);
    }
    
    return (hash >>> 0).toString(); // Convert to unsigned 32-bit integer and then to string
}

/**
 * Manager for multimodal image processing
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
     * Load an image from a buffer
     * 
     * @param imageBuffer The image buffer data
     * @returns MultiBitmap instance
     */
    public loadImageFromBuffer(imageBuffer: Buffer): MultiBitmap {
        const bitmap = this._bindings.initMultimodalBitmapFromBuffer(imageBuffer);
        
        // Automatically generate and set a hash ID for the bitmap
        const hash = createImageHash(bitmap.getData());
        bitmap.setId(hash.toString());
        
        return bitmap;
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
                const bitmap = this.loadImageFromBuffer(imageBuffer);
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
                const bitmap = this.loadImageFromBuffer(imageBuffer);
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
     * Release resources
     */
    public dispose(): void {
        // Multimodal context is now automatically managed per-context
        // No manual cleanup required - resources are automatically released
        // when the context is disposed
    }
}
