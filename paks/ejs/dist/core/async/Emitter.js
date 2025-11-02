/**
 * Emitter - Event emitter
 *
 * Provides event-driven programming support
 * @spec ejs
 * @stability evolving
 */
export class Emitter {
    events;
    constructor() {
        this.events = new Map();
    }
    /**
     * Register an event listener
     * @param eventName Event name or names
     * @param callback Callback function
     * @returns This emitter for chaining
     */
    on(eventName, callback) {
        if (Array.isArray(eventName)) {
            for (const name of eventName) {
                this.on(name, callback);
            }
            return this;
        }
        if (typeof eventName === 'object') {
            for (const [name, cb] of Object.entries(eventName)) {
                this.on(name, cb);
            }
            return this;
        }
        if (!this.events.has(eventName)) {
            this.events.set(eventName, []);
        }
        this.events.get(eventName).push(callback);
        return this;
    }
    /**
     * Register a one-time event listener
     * @param eventName Event name
     * @param callback Callback function
     * @returns This emitter for chaining
     */
    once(eventName, callback) {
        const wrapper = (...args) => {
            callback(...args);
            this.off(eventName, wrapper);
        };
        return this.on(eventName, wrapper);
    }
    /**
     * Remove an event listener
     * @param eventName Event name or names
     * @param callback Callback function to remove
     * @returns This emitter for chaining
     */
    off(eventName, callback) {
        if (Array.isArray(eventName)) {
            for (const name of eventName) {
                this.off(name, callback);
            }
            return this;
        }
        const listeners = this.events.get(eventName);
        if (!listeners) {
            return this;
        }
        const index = listeners.indexOf(callback);
        if (index >= 0) {
            listeners.splice(index, 1);
        }
        if (listeners.length === 0) {
            this.events.delete(eventName);
        }
        return this;
    }
    /**
     * Emit an event
     * @param eventName Event name
     * @param ...args Arguments to pass to listeners
     * @returns True if event had listeners
     */
    emit(eventName, ...args) {
        const listeners = this.events.get(eventName);
        if (!listeners || listeners.length === 0) {
            return false;
        }
        for (const listener of [...listeners]) {
            try {
                listener(...args);
            }
            catch (error) {
                // Emit 'error' event if there are listeners, otherwise log to console
                // Don't emit 'error' for errors in 'error' listeners to avoid infinite loops
                if (eventName !== 'error' && this.events.has('error') && this.events.get('error').length > 0) {
                    this.emit('error', error, eventName);
                }
                else {
                    // Only log to console if no error listeners are registered or if in error listener
                    console.error(`Error in event listener for "${eventName}":`, error);
                }
            }
        }
        return true;
    }
    /**
     * Remove all listeners for an event
     * @param eventName Optional event name (if omitted, removes all)
     */
    removeAllListeners(eventName) {
        if (eventName) {
            this.events.delete(eventName);
        }
        else {
            this.events.clear();
        }
        return this;
    }
    /**
     * Get listener count for an event
     * @param eventName Event name
     * @returns Number of listeners
     */
    listenerCount(eventName) {
        const listeners = this.events.get(eventName);
        return listeners ? listeners.length : 0;
    }
    /**
     * Get all event names
     * @returns Array of event names
     */
    eventNames() {
        return Array.from(this.events.keys());
    }
    /**
     * Emit an event to the registered observers (Ejscript compatibility alias for emit)
     * @param eventName Event name to fire
     * @param args Arguments to pass to listeners
     */
    fire(eventName, ...args) {
        this.emit(eventName, ...args);
    }
    /**
     * Emit an event with an explicit 'this' context for callbacks
     * @param eventName Event name to fire
     * @param thisObj Object to use as 'this' when invoking callbacks
     * @param args Arguments to pass to listeners
     */
    fireThis(eventName, thisObj, ...args) {
        const listeners = this.events.get(eventName);
        if (!listeners || listeners.length === 0) {
            return;
        }
        for (const listener of [...listeners]) {
            try {
                listener.apply(thisObj, args);
            }
            catch (error) {
                // Emit 'error' event if there are listeners, otherwise log to console
                // Don't emit 'error' for errors in 'error' listeners to avoid infinite loops
                if (eventName !== 'error' && this.events.has('error') && this.events.get('error').length > 0) {
                    this.emit('error', error, eventName);
                }
                else {
                    // Only log to console if no error listeners are registered or if in error listener
                    console.error(`Error in event listener for "${eventName}":`, error);
                }
            }
        }
    }
    /**
     * Check if emitter has any observers
     * @returns True if there are registered observers for any event
     */
    hasObservers() {
        for (const listeners of this.events.values()) {
            if (listeners.length > 0) {
                return true;
            }
        }
        return false;
    }
    /**
     * Get all observers for a specific event
     * @param eventName Event name
     * @returns Array of observer callback functions (cloned)
     */
    getObservers(eventName) {
        const listeners = this.events.get(eventName);
        return listeners ? [...listeners] : [];
    }
    /**
     * Clear observers for an event or all events (Ejscript compatibility alias)
     * @param eventName Optional event name (if omitted, clears all)
     */
    clearObservers(eventName) {
        if (eventName === null || eventName === undefined) {
            return this.removeAllListeners();
        }
        if (Array.isArray(eventName)) {
            for (const name of eventName) {
                this.removeAllListeners(name);
            }
        }
        else {
            this.removeAllListeners(eventName);
        }
        return this;
    }
}
//# sourceMappingURL=Emitter.js.map