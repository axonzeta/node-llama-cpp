---
description: Working with multimodal (text+image) models in node-llama-cpp
---

# Multimodal Support {#title}

node-llama-cpp supports multimodal models that can process both text and images. This page explains how to use this functionality.

## Requirements

To use multimodal functionality, you need:

1. A multimodal model (like Llava, GPT-4o, SmolVLM, etc.)
2. The model needs to be in GGUF format with multimodal projections included
3. node-llama-cpp compiled with multimodal support

## Image Token Format

When working with multimodal models, you need to use special image tokens in your prompts to indicate where images should be processed:

- `<__image__>` - Single image token placeholder
- For multiple images: `<__image__><__image__>` - One token per image, in order

Example:
```typescript
// Single image
const prompt = "<__image__>Describe what you see in this image:";
const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, [imageBuffer]);

// Multiple images
const prompt = "<__image__><__image__>Compare these two images:";
const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, [image1Buffer, image2Buffer]);
```

## Basic Usage

There are two main approaches to processing images with text:

1. **Recommended**: Use `tokenizeAndEvaluate()` for automatic processing
2. **Alternative**: Use `tokenize()` for manual token management

### Recommended Approach: tokenizeAndEvaluate()

This method automatically handles multimodal context management and processes the content into the context state:

### TypeScript

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
    
    // Process image with text using tokenizeAndEvaluate - multimodal context is automatically managed
    const prompt = "<__image__>Describe what you see in this image:";
    const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, [imageBuffer]);
    
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
    
    // Process image with text using tokenizeAndEvaluate - multimodal context is automatically managed
    const prompt = "<__image__>Describe what you see in this image:";
    const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, [imageBuffer]);
    
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

### Alternative Approach: Manual tokenize()

For more control over token management, you can use the lower-level `tokenize()` method:

```typescript
// Get tokenization result
const result = await llama.multimodal.tokenize(context, "<__image__>Describe this image:", [imageBuffer]);

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

When you call `multimodal.tokenizeAndEvaluate()`, you'll get a result with the following structure:

```typescript
interface TokenizeAndEvaluateResult {
  tokensProcessed: number;
  newSequenceLength: number;
}
```

When you call `multimodal.tokenize()` (lower-level method), you'll get a result with the following structure:

```typescript
interface MultimodalTokenizeResult {
  chunks: {
    // All tokens (text and image tokens combined)
    tokens: number[];
  }[];
}
```

Example for using the tokenizeAndEvaluate result:

```typescript
// Get the evaluation result (recommended approach)
const prompt = "<__image__>Describe this image:";
const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, [imageBuffer]);

console.log("Tokens processed:", evalResult.tokensProcessed);
console.log("New sequence length:", evalResult.newSequenceLength);

// Context is now ready for text generation
const sequence = context.getSequence();
// Generate response from current state...
```

Example for using the lower-level tokenize method:

```typescript
// Get the tokenization result (lower-level approach)
const result = await llama.multimodal.tokenize(context, "Describe this image:", [imageBuffer]);

// Extract all tokens from chunks
const allTokens = result.chunks.flatMap(chunk => chunk.tokens);

// Set tokens in the sequence
const sequence = context.getSequence();
sequence.setTokens(allTokens);
```

## Advanced Usage

### Image Caching

To improve performance when using the same image multiple times, you can use the image hashing and ID system:

```typescript
// Load image once
const imageBuffer = await fs.promises.readFile(path.join(__dirname, "image.jpg"));
const bitmap = llama.multimodal.loadImageFromBuffer(imageBuffer);

// The hash is automatically set, but you can manually set a custom ID if needed
console.log(bitmap.getId()); // Shows automatically generated hash
bitmap.setId("my-custom-id"); // Optional: set custom ID for reference

// Create a collection to hold multiple images
const bitmaps = llama.multimodal.createBitmapCollection();
bitmaps.addBitmap(bitmap);

// Process multiple images
const anotherImage = await fs.promises.readFile(path.join(__dirname, "second-image.jpg"));
const bitmap2 = llama.multimodal.loadImageFromBuffer(anotherImage);
bitmaps.addBitmap(bitmap2);

// Tokenize with multiple images using tokenizeAndEvaluate
const prompt = "<__image__><__image__>Compare these two images:";
const evalResult = await llama.multimodal.tokenizeAndEvaluate(context, prompt, bitmaps);
```

### Complete Example with Multiple Images

Here's a complete example showing how to process multiple images with a multimodal model:

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
    const result = await llama.multimodal.tokenize(context, prompt, [image1, image2]);
    
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
    const result = await llama.multimodal.tokenize(context, prompt, [image1, image2]);
    
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

### Memory Management

Make sure to dispose of bitmaps and collections when they're no longer needed:

```typescript
bitmap.dispose();
bitmaps.dispose();
```

## API Reference

### `LlamaContextSequence`

#### Multimodal Methods

- `evaluateMultimodalContent(text: string, images: Buffer[], options?: EvaluationOptions): Promise<EvaluationResult>` - Convenience method for multimodal evaluation. Handles the entire multimodal processing pipeline including automatic context management, tokenization, evaluation, and cleanup. The `options` parameter supports the same evaluation options as other evaluation methods (maxTokens, temperature, etc.).

### `MultimodalManager`

The main class for multimodal functionality, accessed through `llama.multimodal`.

#### Methods

- `loadImageFromBuffer(imageBuffer: Buffer): MultiBitmap` - Load an image from a buffer.
- `createBitmapCollection(): MultiBitmaps` - Create a new collection of bitmaps.
- `tokenizeAndEvaluate(context: LlamaContext, text: string, images: Buffer[] | MultiBitmaps): Promise<TokenizeAndEvaluateResult>` - **Recommended method** - Tokenize and evaluate multimodal content in one step. Automatically handles context management and processes both text and images into the context state.
- `tokenize(context: LlamaContext, text: string, images: Buffer[] | MultiBitmaps): Promise<MultimodalTokenizeResult>` - Lower-level method to tokenize text with images. Returns tokens that need to be manually set in a sequence. Multimodal context is automatically managed per-context.

#### Properties

- `llama.hasMultimodal` - Boolean indicating if multimodal support is available
- `model.supportsMultimodal` - Boolean indicating if the loaded model supports multimodal processing

### `MultiBitmap`

Represents an image for multimodal processing.

#### Methods

- `getData(): Buffer` - Get the raw pixel data of the bitmap
- `getDimensions(): { width: number; height: number }` - Get the dimensions of the bitmap
- `getId(): string | null` - Get the ID of the bitmap (used for caching)
- `setId(id: string): void` - Set the ID of the bitmap (useful for custom caching)
- `dispose(): void` - Release resources (important to avoid memory leaks)

### `MultiBitmaps`

Collection of MultiBitmap objects for batch processing.

#### Methods

- `addBitmap(bitmap: MultiBitmap): void` - Add a bitmap to the collection
- `getBitmapCount(): number` - Get the number of bitmaps in the collection
- `dispose(): void` - Release resources for all bitmaps (important to avoid memory leaks)

### `TokenizeAndEvaluateResult`

Result from calling `tokenizeAndEvaluate()`.

#### Properties

- `tokensProcessed: number` - Number of tokens that were processed and added to the context
- `newSequenceLength: number` - The new length of the sequence after processing

### `MultimodalTokenizeResult`

Result from tokenizing text with images using the lower-level `tokenize()` method.

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
