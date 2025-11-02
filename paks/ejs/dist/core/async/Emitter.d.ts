/**
 * Emitter - Event emitter
 *
 * Provides event-driven programming support
 * @spec ejs
 * @stability evolving
 */
export declare class Emitter {
    private events;
    constructor();
    /**
     * Register an event listener
     * @param eventName Event name or names
     * @param callback Callback function
     * @returns This emitter for chaining
     */
    on(eventName: string | string[] | object, callback: Function): this;
    /**
     * Register a one-time event listener
     * @param eventName Event name
     * @param callback Callback function
     * @returns This emitter for chaining
     */
    once(eventName: string, callback: Function): this;
    /**
     * Remove an event listener
     * @param eventName Event name or names
     * @param callback Callback function to remove
     * @returns This emitter for chaining
     */
    off(eventName: string | string[], callback: Function): this;
    /**
     * Emit an event
     * @param eventName Event name
     * @param ...args Arguments to pass to listeners
     * @returns True if event had listeners
     */
    emit(eventName: string, ...args: any[]): boolean;
    /**
     * Remove all listeners for an event
     * @param eventName Optional event name (if omitted, removes all)
     */
    removeAllListeners(eventName?: string): this;
    /**
     * Get listener count for an event
     * @param eventName Event name
     * @returns Number of listeners
     */
    listenerCount(eventName: string): number;
    /**
     * Get all event names
     * @returns Array of event names
     */
    eventNames(): string[];
    /**
     * Emit an event to the registered observers (Ejscript compatibility alias for emit)
     * @param eventName Event name to fire
     * @param args Arguments to pass to listeners
     */
    fire(eventName: string, ...args: any[]): void;
    /**
     * Emit an event with an explicit 'this' context for callbacks
     * @param eventName Event name to fire
     * @param thisObj Object to use as 'this' when invoking callbacks
     * @param args Arguments to pass to listeners
     */
    fireThis(eventName: string, thisObj: any, ...args: any[]): void;
    /**
     * Check if emitter has any observers
     * @returns True if there are registered observers for any event
     */
    hasObservers(): boolean;
    /**
     * Get all observers for a specific event
     * @param eventName Event name
     * @returns Array of observer callback functions (cloned)
     */
    getObservers(eventName: string): Function[];
    /**
     * Clear observers for an event or all events (Ejscript compatibility alias)
     * @param eventName Optional event name (if omitted, clears all)
     */
    clearObservers(eventName?: string | string[] | null): this;
}
//# sourceMappingURL=Emitter.d.ts.map