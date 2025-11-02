/**
 * Worker - Web Worker style threading
 *
 * Provides worker thread functionality
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from './Emitter';
export declare class Worker extends Emitter {
    private worker?;
    private _onerror?;
    private _onmessage?;
    /**
     * Create a worker
     * @param scriptPath Path to worker script
     * @param options Worker options
     */
    constructor(scriptPath: string, options?: any);
    /**
     * Post a message to the worker
     * @param message Message to send
     * @param transfer Optional transferable objects
     */
    postMessage(message: any, transfer?: any[]): void;
    /**
     * Terminate the worker
     */
    terminate(): void;
    /**
     * Get worker thread ID
     */
    get threadId(): number;
    /**
     * Error handler callback
     */
    get onerror(): ((error: Error) => void) | undefined;
    set onerror(handler: ((error: Error) => void) | undefined);
    /**
     * Message handler callback
     */
    get onmessage(): ((message: any) => void) | undefined;
    set onmessage(handler: ((message: any) => void) | undefined);
    /**
     * Evaluate code in the worker context
     * @param code Code to evaluate
     */
    eval(code: string): void;
    /**
     * Pre-evaluate code before worker starts
     * @param code Code to pre-evaluate
     */
    preeval(code: string): void;
    /**
     * Load a script into the worker
     * @param scriptPath Path to script
     */
    load(scriptPath: string): void;
    /**
     * Pre-load a script before worker starts
     * @param scriptPath Path to script
     */
    preload(scriptPath: string): void;
    /**
     * Exit the worker (from within the worker)
     * @param code Exit code
     */
    exit(code?: number): void;
    /**
     * Clone the worker
     * @returns New worker with same script
     */
    clone(): Worker;
    /**
     * Wait for a message from the worker
     * @param timeout Timeout in milliseconds
     * @returns Promise that resolves with the message
     */
    waitForMessage(timeout?: number): Promise<any>;
}
//# sourceMappingURL=Worker.d.ts.map