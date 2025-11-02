/**
 * MprLog - MPR logging subsystem
 *
 * Provides low-level access to application log file
 * @spec ejs
 * @stability evolving
 */
import { Path } from '../Path';
export class MprLog {
    _level = 0;
    _location = 'stderr';
    _fixed = false;
    /**
     * Get/set log level
     */
    get level() {
        return this._level;
    }
    set level(value) {
        this._level = value;
    }
    /**
     * Get/set log location
     */
    get location() {
        return this._location;
    }
    set location(value) {
        this._location = value;
    }
    /**
     * Check if log is fixed (set via command line)
     */
    get fixed() {
        return this._fixed;
    }
    /**
     * Redirect log output
     * @param location New output location
     * @param level New log level
     */
    redirect(location, level) {
        if (!this._fixed) {
            this._location = location;
            this._level = level;
        }
    }
    /**
     * Write to the log
     * @param ...args Message parts
     */
    write(...args) {
        const message = args.join(' ');
        const timestamp = new Date().toISOString();
        const logLine = `[${timestamp}] ${message}\n`;
        if (this._location === 'stdout') {
            process.stdout.write(logLine);
        }
        else if (this._location === 'stderr') {
            process.stderr.write(logLine);
        }
        else {
            // Write to file
            const path = new Path(this._location);
            path.append(logLine);
        }
    }
}
//# sourceMappingURL=MprLog.js.map