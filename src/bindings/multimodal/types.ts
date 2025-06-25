/**
 * MultiBitmap class representing an image or audio for multimodal LLM processing
 */
export interface MultiBitmap {
    /**
     * Get the raw data of the bitmap
     * @returns Buffer containing RGB pixel data for images or Float32 PCM audio data for audio
     */
    getData(): Buffer;
    
    /**
     * Get the dimensions of the bitmap (for images only)
     * @returns Object containing width and height for images, undefined for audio
     */
    getDimensions(): { width: number; height: number };
    
    /**
     * Get the ID of the bitmap (used for caching)
     * @returns The bitmap ID or null if not set
     */
    getId(): string | null;
    
    /**
     * Set the ID of the bitmap (used for caching)
     * @param id The ID to assign to the bitmap
     */
    setId(id: string): void;
    
    /**
     * Check if this bitmap represents audio data
     * @returns true if this is audio data, false if it's image data
     */
    isAudio(): boolean;
    
    /**
     * Extract tokens from this bitmap with the given context text
     * @param context The LlamaContext to use for tokenization
     * @param text The context text for tokenization
     * @returns Array of token IDs for this media object
     */
    getTokens(context: any, text: string): number[];
    
    /**
     * Release resources associated with this bitmap
     */
    dispose(): void;
}

/**
 * Collection of MultiBitmap objects (images and/or audio) for multimodal processing
 */
export interface MultiBitmaps {
    /**
     * Add a bitmap (image or audio) to the collection
     * @param bitmap The bitmap to add
     */
    addBitmap(bitmap: MultiBitmap): void;
    
    /**
     * Get the number of bitmaps in the collection
     * @returns The bitmap count
     */
    getBitmapCount(): number;
    
    /**
     * Release resources associated with all bitmaps
     */
    dispose(): void;
}

/**
 * Result from tokenizing a text with images and/or audio
 */
export interface MultimodalTokenizeResult {
    /**
     * The tokenized chunks
     */
    chunks: {
        /**
         * Text tokens
         */
        tokens: number[];
        
        /**
         * Media tokens (image or audio, if applicable)
         */
        imageTokens?: number[];
    }[];
    
    /**
     * All tokens as a flat array (new field)
     */
    tokens: number[];
}

/**
 * Vision encoder state for caching processed media
 */
export interface VisionEncoderState {
    /**
     * Type identifier for the state object
     */
    type: "visionEncoderState";
    
    /**
     * Version of the state format
     */
    version: string;
    
    /**
     * Width of the original image
     */
    width: number;
    
    /**
     * Height of the original image
     */
    height: number;
    
    /**
     * Bitmap ID for identification
     */
    bitmapId?: string;
    
    /**
     * Raw bitmap data (RGB format)
     */
    bitmapData: Buffer;
    
    /**
     * Timestamp when the state was created
     */
    timestamp: number;
    
    /**
     * Model identifier for compatibility checking
     */
    modelId: string;
}
