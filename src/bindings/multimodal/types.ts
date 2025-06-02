/**
 * MultiBitmap class representing an image for multimodal LLM processing
 */
export interface MultiBitmap {
    /**
     * Get the raw pixel data of the bitmap
     * @returns Buffer containing RGB pixel data
     */
    getData(): Buffer;
    
    /**
     * Get the dimensions of the bitmap
     * @returns Object containing width and height
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
     * Release resources associated with this bitmap
     */
    dispose(): void;
}

/**
 * Collection of MultiBitmap objects for multimodal processing
 */
export interface MultiBitmaps {
    /**
     * Add a bitmap to the collection
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
 * Result from tokenizing a text with images
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
         * Image tokens (if applicable)
         */
        imageTokens?: number[];
    }[];
}
