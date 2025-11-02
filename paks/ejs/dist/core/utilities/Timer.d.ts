/**
 * Timer - Timer and callback management
 *
 * Provides timer functionality with callbacks
 * @spec ejs
 * @stability evolving
 */
import { Emitter } from '../async/Emitter';
export declare class Timer extends Emitter {
    private _period;
    private _drift;
    private _repeat;
    private _callback?;
    private _callbackArgs;
    private _onerror?;
    private timerId?;
    private intervalId?;
    private _isRunning;
    /**
     * Create a timer. The timer will not run until start() is called.
     * @param period Timer period in milliseconds
     * @param callback Callback function to invoke when timer fires
     * @param args Arguments to pass to the callback
     */
    constructor(period: number, callback?: Function, ...args: any[]);
    /**
     * Get timer period in milliseconds
     */
    get period(): number;
    /**
     * Set timer period in milliseconds
     * Changing the period does not reschedule a currently running timer.
     * The new period will be used for the next invocation.
     */
    set period(value: number);
    /**
     * Get drift setting
     * If true, timer is allowed to drift its execution time
     */
    get drift(): boolean;
    /**
     * Set drift setting
     * If false, timer is scheduled to not drift (period between start times is constant)
     */
    set drift(enable: boolean);
    /**
     * Get repeat setting
     * If true, timer will be repeatedly invoked
     */
    get repeat(): boolean;
    /**
     * Set repeat setting
     * If true, timer will be repeatedly invoked every period milliseconds
     */
    set repeat(enable: boolean);
    /**
     * Get error callback function
     * Called when an exception occurs inside the timer callback
     */
    get onerror(): Function | undefined;
    /**
     * Set error callback function
     * The callback is invoked with signature: function callback(error: Error): void
     */
    set onerror(callback: Function | undefined);
    /**
     * Start the timer running
     * The timer will be repeatedly invoked if repeat property is true
     * @returns The timer instance for chaining
     */
    start(): Timer;
    /**
     * Stop the timer
     * Once stopped, the timer can be restarted by calling start()
     */
    stop(): void;
    /**
     * Reschedule the timer
     * Stops the timer if currently scheduled, then reschedules it
     * @param when Optional new period. If provided, sets the period before starting
     */
    restart(when?: number): void;
    /**
     * Fire the timer callback
     */
    private triggerCallback;
    /**
     * Schedule repeating timer without drift
     * Compensates for execution time to maintain constant period between starts
     */
    private scheduleRepeating;
    /**
     * Create a one-shot timer
     * @param delay Delay in milliseconds
     * @param callback Callback function
     * @returns Timer instance
     */
    static after(delay: number, callback: Function): Timer;
}
//# sourceMappingURL=Timer.d.ts.map