---
description: Working with multimodal (text+image) models in node-llama-cpp
---

# Multimodal Support {#title}

node-llama-cpp supports multimodal models that can process text, images, and audio. This page explains how to use this functionality.

## Requirements

To use multimodal functionality, you need:

1. A multimodal model (like Llava, GPT-4o, SmolVLM, Qwen2.5-Omni, etc.)
2. The model needs to be in GGUF format with multimodal projections included
3. node-llama-cpp compiled with multimodal support

## Media Token Format

When working with multimodal models, you need to use special media tokens in your prompts to indicate where images and audio should be processed:

- `<__media__>` - Universal media token placeholder for images or audio
- For multiple media files: `<__media__><__media__>` - One token per media file, in order

Example:
```typescript
// Single image
const prompt = "<__media__>Describe what you see in this image:";
const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [imageBuffer]);

// Single audio
const prompt = "<__media__>Describe what you hear in this audio:";
const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [audioBuffer]);

// Multiple media files
const prompt = "<__media__><__media__>Compare this image and audio:";
const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [imageBuffer, audioBuffer]);
```

::: tip Legacy Image Token Support
The older `<__image__>` token format is still supported for backward compatibility, but `<__media__>` is recommended for new implementations as it works with both images and audio.
:::

## Basic Usage

There are two main approaches to processing media (images and audio) with text:

1. **Recommended**: Use `tokenizeAndEvaluateMedia()` for automatic processing
2. **Alternative**: Use `tokenizeMedia()` for manual token management

### Recommended Approach: tokenizeAndEvaluateMedia()

This method automatically handles multimodal context management and processes the content into the context state:

### TypeScript

#### Image Processing Example

```typescript
import {getLlama, LlamaContext} from "node-llama-cpp"; // Ensure LlamaContext is imported
import fs from "fs";
import path from "path";

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf"; // Required for multimodal context

  let context: LlamaContext | null = null;
  // Note: Proper error handling and resource management (model, context disposal)
  // are important in production applications.

  try {
    // Load a multimodal model
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    // Create context
    context = await model.createContext();
    
    // No manual initialization needed - multimodal context is automatically managed
    // per-context and initialized when needed during tokenization
    
    // Load image
    const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
    
    // Process image with text using tokenizeAndEvaluateMedia - multimodal context is automatically managed
    const prompt = "<__media__>Describe what you see in this image:";
    const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [imageBuffer]);
    
    console.log("Tokens processed:", evalResult.tokensProcessed);
    console.log("New sequence length:", evalResult.newSequenceLength);
    
    // Generate response from the current context state
    const sequence = context.getSequence();
    let generatedText = "";
    let tokenCount = 0;
    const maxTokens = 512;
    
    // Generate tokens one by one
    const promptTokens = context.model.tokenize("\n");
    const responseTokens = sequence.evaluate(promptTokens, { maxTokens });
    
    for await (const token of responseTokens) {
      const tokenText = context.model.detokenize([token]);
      generatedText += tokenText;
      process.stdout.write(tokenText);
      
      tokenCount++;
      
      // Stop at natural ending points or max tokens
      if (tokenCount >= maxTokens || 
          tokenText.includes('<|end_of_turn|>') || 
          tokenText.includes('</s>') ||
          tokenText.includes('<|eot_id|>')) {
        break;
      }
    }
    
    console.log(`\nGeneration completed. Generated ${tokenCount} tokens.`);

  } catch (error) {
    console.error("An error occurred:", error);
  } finally {
    // No manual cleanup needed - multimodal context is automatically
    // released when the context is disposed
    
    // Optional: Dispose of LlamaContext if it was created
    // if (context) {
    //   await context.dispose();
    // }
    // Consider disposing the model as well if appropriate for your application lifecycle.
  }
}

main();
```

#### Audio Processing Example

```typescript
import {getLlama, LlamaContext} from "node-llama-cpp";
import fs from "fs";
import path from "path";

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/Qwen2.5-Omni-3B-Q4_K_M.gguf"; // Audio-capable model
  const multimodalProjectorPath = "path/to/mmproj-Qwen2.5-Omni-3B-Q8_0.gguf"; // Required for audio support

  let context: LlamaContext | null = null;

  try {
    // Load a multimodal model with audio support
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });

    // Create context
    context = await model.createContext();

    // Load audio file
    const audioBuffer = await fs.promises.readFile(path.join(__dirname, "audio.mp3"));

    console.log("Processing multimodal input...");
    console.log("Audio buffer size:", audioBuffer.length);
    
    // Use the correct prompt format for audio input
    const prompt = "<__media__>Describe what you hear in this audio:";
    console.log("Text prompt:", prompt);
    
    // First, let's test the bitmap creation directly
    console.log("Testing bitmap creation...");
    const bitmap = llama.multimodal.loadMediaFromBuffer(context, audioBuffer);
    console.log("Bitmap created, isAudio():", bitmap.isAudio());
    console.log("Bitmap ID:", bitmap.getId());
    
    // Clean up the test bitmap
    bitmap.dispose();
    
    // Process audio with text using tokenizeAndEvaluateMedia
    const evalResult = llama.multimodal.tokenizeAndEvaluateMedia(
      context,
      prompt,
      [audioBuffer]
    );
    
    console.log("Evaluation result:", evalResult);
    console.log("Tokens processed:", evalResult.tokensProcessed);
    console.log("New sequence length:", evalResult.newSequenceLength);

    // Generate response from the current context state
    console.log("Starting text generation...");
    
    try {
      // Get a sequence that starts from the current context state
      const sequence = context.getSequence();
      
      console.log("Generating response:");
      
      // Generate from the current state
      const promptTokens = context.model.tokenize("\n");
      const responseTokens = sequence.evaluate(promptTokens, { maxTokens: 100 });
      
      let generatedText = "";
      let tokenCount = 0;
      
      for await (const token of responseTokens) {
        const tokenText = context.model.detokenize([token]);
        generatedText += tokenText;
        process.stdout.write(tokenText);
        
        tokenCount++;
        
        // Stop at natural ending points or max tokens
        if (tokenCount >= 100 || 
            tokenText.includes('<|end_of_turn|>') || 
            tokenText.includes('</s>') ||
            tokenText.includes('<|eot_id|>')) {
          break;
        }
      }
      
      console.log(`\n\nGeneration completed. Generated ${tokenCount} tokens.`);
      console.log("Full response:", generatedText);
      
    } catch (generationError) {
      console.error("Generation failed:", generationError);
    }
  } catch (error) {
    console.error("An error occurred:", error);
  } 
}

main();
```

### JavaScript

```javascript
const {getLlama} = require("node-llama-cpp");
const fs = require("fs");
const path = require("path");

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf"; // Required for multimodal context

  let context = null;
  // Note: Proper error handling and resource management (model, context disposal)
  // are important in production applications.

  try {
    // Load a multimodal model
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    // Create context
    context = await model.createContext();
    
    // No manual initialization needed - multimodal context is automatically managed
    // per-context and initialized when needed during tokenization
    
    // Load image
    const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
    
    // Process image with text using tokenizeAndEvaluateMedia - multimodal context is automatically managed
    const prompt = "<__media__>Describe what you see in this image:";
    const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [imageBuffer]);
    
    console.log("Tokens processed:", evalResult.tokensProcessed);
    console.log("New sequence length:", evalResult.newSequenceLength);
    
    // Generate response from the current context state
    const sequence = context.getSequence();
    let generatedText = "";
    let tokenCount = 0;
    const maxTokens = 512;
    
    // Generate tokens one by one
    const promptTokens = context.model.tokenize("\n");
    const responseTokens = sequence.evaluate(promptTokens, { maxTokens });
    
    for await (const token of responseTokens) {
      const tokenText = context.model.detokenize([token]);
      generatedText += tokenText;
      process.stdout.write(tokenText);
      
      tokenCount++;
      
      // Stop at natural ending points or max tokens
      if (tokenCount >= maxTokens || 
          tokenText.includes('<|end_of_turn|>') || 
          tokenText.includes('</s>') ||
          tokenText.includes('<|eot_id|>')) {
        break;
      }
    }
    
    console.log(`\nGeneration completed. Generated ${tokenCount} tokens.`);

  } catch (error) {
    console.error("An error occurred:", error);
  } finally {
    // No manual cleanup needed - multimodal context is automatically
    // released when the context is disposed
    
    // Optional: Dispose of LlamaContext if it was created
    // if (context) {
    //   await context.dispose();
    // }
    // Consider disposing the model as well if appropriate for your application lifecycle.
  }
}

main();
```

### Alternative Approach: Manual tokenizeMedia()

For more control over token management, you can use the lower-level `tokenizeMedia()` method:

```typescript
// Get tokenization result
const result = await llama.multimodal.tokenizeMedia(context, "<__media__>Describe this image:", [imageBuffer]);

// Extract and set tokens manually
const allTokens = result.chunks.flatMap(chunk => chunk.tokens);
const sequence = context.getSequence();
sequence.setTokens(allTokens);

// Evaluate manually
const response = await sequence.evaluate({ maxTokens: 512 });
console.log(response.tokens.text);
```

## Convenience Method for Multimodal Evaluation

For easier multimodal processing, you can use the convenience method `evaluateMultimodalContent()` on `LlamaContextSequence`:

### TypeScript

```typescript
import {getLlama} from "node-llama-cpp";
import fs from "fs";
import path from "path";

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf";

  try {
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    const context = await model.createContext();
    const sequence = context.getSequence();
    
    // Load image
    const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
    
    // Use the convenience method for multimodal evaluation
    const result = await sequence.evaluateMultimodalContent(
      "Describe what you see in this image:",
      [imageBuffer],
      {
        maxTokens: 512,
        temperature: 0.7
      }
    );
    
    console.log("AI Response:");
    console.log(result.response);

  } catch (error) {
    console.error("An error occurred:", error);
  }
}

main();
```

### JavaScript

```javascript
const {getLlama} = require("node-llama-cpp");
const fs = require("fs");
const path = require("path");

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf";

  try {
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    const context = await model.createContext();
    const sequence = context.getSequence();
    
    // Load image
    const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
    
    // Use the convenience method for multimodal evaluation
    const result = await sequence.evaluateMultimodalContent(
      "Describe what you see in this image:",
      [imageBuffer],
      {
        maxTokens: 512,
        temperature: 0.7
      }
    );
    
    console.log("AI Response:");
    console.log(result.response);

  } catch (error) {
    console.error("An error occurred:", error);
  }
}

main();
```

This convenience method handles all the multimodal setup internally, including:
- Automatically managing the multimodal context per-context
- Tokenizing the text and images
- Setting tokens in the sequence
- Evaluating the model
- Cleaning up resources

## Response Format

When you call `multimodal.tokenizeAndEvaluateMedia()`, you'll get a result with the following structure:

```typescript
interface TokenizeAndEvaluateResult {
  tokensProcessed: number;
  newSequenceLength: number;
}
```

When you call `multimodal.tokenizeMedia()` (lower-level method), you'll get a result with the following structure:

```typescript
interface MultimodalTokenizeResult {
  chunks: {
    // All tokens (text and image tokens combined)
    tokens: number[];
  }[];
}
```

Example for using the tokenizeAndEvaluateMedia result:

```typescript
// Get the evaluation result (recommended approach)
const prompt = "<__media__>Describe this image:";
const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, [imageBuffer]);

console.log("Tokens processed:", evalResult.tokensProcessed);
console.log("New sequence length:", evalResult.newSequenceLength);

// Context is now ready for text generation
const sequence = context.getSequence();
// Generate response from current state...
```

Example for using the lower-level tokenizeMedia method:

```typescript
// Get the tokenization result (lower-level approach)
const result = await llama.multimodal.tokenizeMedia(context, "Describe this image:", [imageBuffer]);

// Extract all tokens from chunks
const allTokens = result.chunks.flatMap(chunk => chunk.tokens);

// Set tokens in the sequence
const sequence = context.getSequence();
sequence.setTokens(allTokens);
```

## Advanced Usage

### Media Caching

To improve performance when using the same media files multiple times, you can use the media hashing and ID system:

```typescript
// Load image once
const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
const bitmap = llama.multimodal.loadMediaFromBuffer(context, imageBuffer);

// The hash is automatically set, but you can manually set a custom ID if needed
console.log(bitmap.getId()); // Shows automatically generated hash
bitmap.setId("my-custom-id"); // Optional: set custom ID for reference

// Create a collection to hold multiple media files
const bitmaps = llama.multimodal.createBitmapCollection();
bitmaps.addBitmap(bitmap);

// Process multiple media files
const anotherImage = await fs.promises.readFile(path.join(__dirname, "second-image.jpg"));
const bitmap2 = llama.multimodal.loadMediaFromBuffer(context, anotherImage);
bitmaps.addBitmap(bitmap2);

// Tokenize with multiple media files using tokenizeAndEvaluateMedia
const prompt = "<__media__><__media__>Compare these two images:";
const evalResult = await llama.multimodal.tokenizeAndEvaluateMedia(context, prompt, bitmaps);
```

### Complete Example with Multiple Media Files

Here's a complete example showing how to process multiple media files with a multimodal model:

### TypeScript

```typescript
import {getLlama, LlamaContext} from "node-llama-cpp"; // Ensure LlamaContext is imported
import fs from "fs";
import path from "path";

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf"; // Required

  let context: LlamaContext | null = null;

  try {
    console.log("Multimodal support available:", llama.hasMultimodal);
    
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    console.log("Model supports multimodal:", model.supportsMultimodal);
    
    context = await model.createContext();
    
    // No manual initialization needed - multimodal context is automatically
    // managed per-context and initialized when needed during tokenization
    
    // Load multiple images
    const image1 = await fs.promises.readFile(path.join(__dirname, "cat.jpg"));
    const image2 = await fs.promises.readFile(path.join(__dirname, "dog.jpg"));
    
    const prompt = "Compare these two images. What are the main differences between them?";
    // Tokenize - multimodal context is automatically managed
    const result = await llama.multimodal.tokenizeMedia(context, prompt, [image1, image2]);
    
    const allTokens = result.chunks.flatMap(chunk => chunk.tokens);
    
    const sequence = context.getSequence();
    sequence.setTokens(allTokens);
    
    const response = await sequence.evaluate({
      maxTokens: 512,
      temperature: 0.7
    });
    
    console.log("AI Response:");
    console.log(response.tokens.text);

  } catch (error) {
    console.error("An error occurred in multimodal example:", error);
  } finally {
    // No manual cleanup needed - multimodal context is automatically
    // released when the context is disposed

    // Optional: Dispose context
    // if (context) {
    //   await context.dispose();
    // }
  }
}

main();
```

### JavaScript

```javascript
const {getLlama} = require("node-llama-cpp");
const fs = require("fs");
const path = require("path");

async function main() {
  const llama = await getLlama();
  const modelPath = "path/to/llava-v1.5-7b-q4_k.gguf";
  const multimodalProjectorPath = "path/to/mmproj-model-f16.gguf"; // Required

  let context = null;

  try {
    console.log("Multimodal support available:", llama.hasMultimodal);
    
    const model = await llama.loadModel({
      modelPath: modelPath,
      multimodalProjectorPath: multimodalProjectorPath
    });
    
    console.log("Model supports multimodal:", model.supportsMultimodal);
    
    context = await model.createContext();
    
    // No manual initialization needed - multimodal context is automatically
    // managed per-context and initialized when needed during tokenization
    
    // Load multiple images
    const image1 = await fs.promises.readFile(path.join(__dirname, "cat.jpg"));
    const image2 = await fs.promises.readFile(path.join(__dirname, "dog.jpg"));
    
    const prompt = "Compare these two images. What are the main differences between them?";
    // Tokenize - multimodal context is automatically managed
    const result = await llama.multimodal.tokenizeMedia(context, prompt, [image1, image2]);
    
    const allTokens = result.chunks.flatMap(chunk => chunk.tokens);
    
    const sequence = context.getSequence();
    sequence.setTokens(allTokens);
    
    const response = await sequence.evaluate({
      maxTokens: 512,
      temperature: 0.7
    });
    
    console.log("AI Response:");
    console.log(response.tokens.text);

  } catch (error) {
    console.error("An error occurred in multimodal example:", error);
  } finally {
    // No manual cleanup needed - multimodal context is automatically
    // released when the context is disposed

    // Optional: Dispose context
    // if (context) {
    //   await context.dispose();
    // }
  }
}

main();
```

### Token Extraction for Advanced Use Cases

For advanced use cases where you need direct access to tokens from media objects, you can extract tokens that can be used in subsequent calls:

```typescript
// Method 1: Extract all tokens as a flat array from multimodal input
const tokens = await llama.multimodal.getTokens(context, "Describe this image:", [imageBuffer]);
console.log("Extracted tokens:", tokens);

// Use tokens in sequence evaluation
const sequence = context.getSequence();
sequence.setTokens(tokens);

// Method 2: Get tokens from tokenize result (includes both chunks and flat array)
const tokenizeResult = await llama.multimodal.tokenizeMedia(context, "Describe this image:", [imageBuffer]);
console.log("Flat tokens:", tokenizeResult.tokens); // New: flat array of all tokens
console.log("Detailed chunks:", tokenizeResult.chunks); // Original: chunk structure

// Method 3: Extract tokens from individual media objects
const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
const bitmap = llama.multimodal.loadMediaFromBuffer(context, imageBuffer);
bitmap.setId("my-image-1");

// Get tokens from this specific bitmap
const bitmapTokens = bitmap.getTokens(context, "What do you see?");
console.log("Bitmap tokens:", bitmapTokens);

// Combine with other tokens
const textTokens = model.tokenize("Additional context: ");
const combinedTokens = [...textTokens, ...bitmapTokens];
sequence.setTokens(combinedTokens);
```

**Token Types:**
- **Text tokens**: Actual token IDs from the model's vocabulary
- **Image tokens**: Placeholder value `-1` (actual embeddings computed during evaluation)
- **Audio tokens**: Placeholder value `-2` (actual embeddings computed during evaluation)

**Use Cases:**
- Caching tokenized media for reuse across multiple contexts
- Manual token manipulation and combination
- Building custom evaluation pipelines
- Token-level analysis and debugging

For detailed examples and API reference, see the [Token Extraction Guide](../examples/multimodal-token-extraction.md).

## API Reference

### `LlamaContextSequence`

#### Multimodal Methods

- `evaluateMultimodalContent(text: string, images: Buffer[], options?: EvaluationOptions): Promise<EvaluationResult>` - Convenience method for multimodal evaluation. Handles the entire multimodal processing pipeline including automatic context management, tokenization, evaluation, and cleanup. The `options` parameter supports the same evaluation options as other evaluation methods (maxTokens, temperature, etc.).

### `MultimodalManager`

The main class for multimodal functionality, accessed through `llama.multimodal`.

#### Methods

- `loadMediaFromBuffer(context: LlamaContext, mediaBuffer: Buffer): MultiBitmap` - Load media (image or audio) from a buffer. Automatically detects the media type.
- `loadImageFromBuffer(imageBuffer: Buffer): MultiBitmap` - Load an image from a buffer (deprecated, use loadMediaFromBuffer).
- `createBitmapCollection(): MultiBitmaps` - Create a new collection of bitmaps.
- `tokenizeAndEvaluateMedia(context: LlamaContext, text: string, media: Buffer[] | MultiBitmaps): Promise<TokenizeAndEvaluateResult>` - **Recommended method** - Tokenize and evaluate multimodal content in one step. Automatically handles context management and processes both text and media into the context state.
- `tokenizeMedia(context: LlamaContext, text: string, media: Buffer[] | MultiBitmaps): Promise<MultimodalTokenizeResult>` - Lower-level method to tokenize text with media. Returns tokens that need to be manually set in a sequence. Multimodal context is automatically managed per-context.

#### Properties

- `llama.hasMultimodal` - Boolean indicating if multimodal support is available
- `model.supportsMultimodal` - Boolean indicating if the loaded model supports multimodal processing

### `MultiBitmap`

Represents media (image or audio) for multimodal processing.

#### Methods

- `getData(): Buffer` - Get the raw data of the bitmap (RGB pixel data for images or Float32 PCM audio data for audio)
- `getDimensions(): { width: number; height: number }` - Get the dimensions of the bitmap (for images only, undefined for audio)
- `getId(): string | null` - Get the ID of the bitmap (used for caching)
- `setId(id: string): void` - Set the ID of the bitmap (useful for custom caching)
- `isAudio(): boolean` - Check if this bitmap represents audio data
- `dispose(): void` - Release resources (important to avoid memory leaks)

### `MultiBitmaps`

Collection of MultiBitmap objects for batch processing.

#### Methods

- `addBitmap(bitmap: MultiBitmap): void` - Add a bitmap to the collection
- `getBitmapCount(): number` - Get the number of bitmaps in the collection
- `dispose(): void` - Release resources for all bitmaps (important to avoid memory leaks)

### `TokenizeAndEvaluateResult`

Result from calling `tokenizeAndEvaluateMedia()`.

#### Properties

- `tokensProcessed: number` - Number of tokens that were processed and added to the context
- `newSequenceLength: number` - The new length of the sequence after processing

### `MultimodalTokenizeResult`

Result from tokenizing text with media using the lower-level `tokenizeMedia()` method.

#### Properties

- `chunks` - Array of tokenized chunks, each containing:
  - `tokens` - Array of tokens (text and image tokens combined)

### Model Loading Options

Additional options when loading a model:

```typescript
interface LlamaModelOptions {
  // ... other options
  
  /**
   * Path to the multimodal projector file (required for multimodal models)
   */
  multimodalProjectorPath?: string;
}
```
