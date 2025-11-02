/**
 * Worker - Web Worker style threading
 *
 * Provides worker thread functionality
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from './Emitter';
import { Worker as BunWorker } from 'worker_threads';
export class Worker extends Emitter {
    worker;
    _onerror;
    _onmessage;
    /**
     * Create a worker
     * @param scriptPath Path to worker script
     * @param options Worker options
     */
    constructor(scriptPath, options) {
        super();
        this.worker = new BunWorker(scriptPath, options);
        this.worker.on('message', (data) => {
            if (this._onmessage) {
                this._onmessage(data);
            }
            this.emit('message', data);
        });
        this.worker.on('error', (error) => {
            if (this._onerror) {
                this._onerror(error);
            }
            this.emit('error', error);
        });
        this.worker.on('exit', (code) => {
            this.emit('exit', code);
        });
    }
    /**
     * Post a message to the worker
     * @param message Message to send
     * @param transfer Optional transferable objects
     */
    postMessage(message, transfer) {
        if (this.worker) {
            this.worker.postMessage(message, transfer);
        }
    }
    /**
     * Terminate the worker
     */
    terminate() {
        if (this.worker) {
            this.worker.terminate();
        }
    }
    /**
     * Get worker thread ID
     */
    get threadId() {
        return this.worker?.threadId ?? -1;
    }
    /**
     * Error handler callback
     */
    get onerror() {
        return this._onerror;
    }
    set onerror(handler) {
        this._onerror = handler;
    }
    /**
     * Message handler callback
     */
    get onmessage() {
        return this._onmessage;
    }
    set onmessage(handler) {
        this._onmessage = handler;
    }
    /**
     * Evaluate code in the worker context
     * @param code Code to evaluate
     */
    eval(code) {
        this.postMessage({ type: 'eval', code });
    }
    /**
     * Pre-evaluate code before worker starts
     * @param code Code to pre-evaluate
     */
    preeval(code) {
        // In Bun, we can't pre-eval, so we'll send it immediately
        this.eval(code);
    }
    /**
     * Load a script into the worker
     * @param scriptPath Path to script
     */
    load(scriptPath) {
        this.postMessage({ type: 'load', scriptPath });
    }
    /**
     * Pre-load a script before worker starts
     * @param scriptPath Path to script
     */
    preload(scriptPath) {
        // In Bun, we can't pre-load, so we'll send it immediately
        this.load(scriptPath);
    }
    /**
     * Exit the worker (from within the worker)
     * @param code Exit code
     */
    exit(code = 0) {
        this.postMessage({ type: 'exit', code });
        // Don't call terminate() - let the worker handle the exit message
        // and exit with the proper code
    }
    /**
     * Clone the worker
     * @returns New worker with same script
     */
    clone() {
        // We can't truly clone in Bun, but we can create a new instance
        // This requires storing the script path, which we don't have access to
        throw new Error('Worker.clone() not supported in Bun environment');
    }
    /**
     * Wait for a message from the worker
     * @param timeout Timeout in milliseconds
     * @returns Promise that resolves with the message
     */
    async waitForMessage(timeout = 30000) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                reject(new Error('Timeout waiting for message'));
            }, timeout);
            const handler = (message) => {
                clearTimeout(timer);
                this.off('message', handler);
                resolve(message);
            };
            this.on('message', handler);
        });
    }
}
//# sourceMappingURL=Worker.js.map