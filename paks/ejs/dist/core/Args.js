/**
 * Args - Command-line argument parsing
 *
 * The Args class parses the Application's command line. A template of permissible command line options
 * is passed to the Args constructor. The Args class supports command options that begin with "-" or "--" and parses
 * option arguments of the forms: "-flag x" and "-flag=x". An option may have aliased forms (i.e. -v for --verbose).
 *
 * The command template contains properties for options, usage message, and error handling policy should
 * invalid options or arguments be submitted. The template can provide a range of valid values for a command option.
 * The option range may be an Ejscript type, regular expression or set of values that constrains the permissible
 * option argument values. A default value may be provided for each option argument.
 *
 * Once parsed, the arguments are accessible via args.options. The remaining command line arguments are available
 * in args.rest.
 *
 * @spec ejs
 * @stability prototype
 *
 * @example
 * ```typescript
 * let args = new Args({
 *     options: {
 *         depth: { range: Number },
 *         quiet: { },
 *         verbose: { alias: 'v', value: true },
 *         log: { range: /\w+(:\d)/, value: 'stderr:4' },
 *         mode: { range: ['low', 'medium', 'high'], value: 'high' },
 *         with: { range: String, separator: ',' },
 *     },
 *     usage: usage,
 *     onerror: 'exit',
 * }, [])
 *
 * let options = args.options
 * if (options.verbose) { }
 * for (let file of args.rest) {
 *     ...
 * }
 * ```
 */
import { Path } from './Path';
import { App } from './App';
/**
 * Args - Command-line argument parser
 *
 * Parses command-line arguments according to a template specification
 */
export class Args {
    /** Program name from App.args[0] */
    program;
    /** Remaining arguments that follow the command options */
    rest = [];
    /** Parsed command line options */
    options = {};
    /** Copy of the argument template specification */
    template;
    /**
     * The Args constructor creates a new instance of the Args class. It parses the command line
     * and stores the parsed options in the options and rest properties.
     * Args supports command options that begin with "-" or "--" and parses option arguments of the forms:
     * "-flag x" and "-flag=x".
     *
     * @param template Command argument template
     * @param argv Array of command arguments to parse. Defaults to App.args.
     *
     * Template options:
     * - options: Object with properties for each command line option. Each option can have:
     *   - alias: String alias for the option (typically single character)
     *   - range: Permissible values (Type, RegExp, or array of values)
     *   - value: Default value for the option
     *   - separator: If defined, allows multiple values (string for concatenation, Array for array)
     * - usage: Function to invoke for argument parse errors
     * - onerror: 'throw' to throw exceptions, 'exit' to exit with non-zero status
     * - silent: Don't emit any message on argument parse errors
     * - unknown: Callback to invoke for unknown arguments
     */
    constructor(template, argv = App.args) {
        // Clone template to avoid modifying original
        this.template = this.cloneTemplate(template);
        // Setup aliases - map alias names to their parent option
        for (const item of Object.values(this.template.options)) {
            if (item.alias) {
                this.template.options[item.alias] = item;
            }
        }
        try {
            this.program = new Path(argv[0]);
            // Parse command line arguments
            for (let i = 1; i < argv.length; i++) {
                const arg = argv[i];
                if (arg.startsWith('-')) {
                    // Extract option name
                    const name = arg.slice(arg.startsWith('--') ? 2 : 1);
                    const [key, equalValue] = name.split('=');
                    const item = this.template.options[key];
                    if (!item) {
                        // Unknown option
                        if (this.template.unknown) {
                            i = this.template.unknown(argv, i);
                            continue;
                        }
                        else if (key === '?') {
                            if (this.template.usage) {
                                this.template.usage();
                                break;
                            }
                        }
                        else {
                            throw new Error(`Undefined option '${key}'`);
                        }
                    }
                    else {
                        let value = equalValue;
                        if (!value) {
                            if (!item.range) {
                                // Flag with no argument
                                value = true;
                            }
                            else {
                                // Option requires an argument
                                if (++i >= argv.length) {
                                    throw new Error(`Missing option for ${key}`);
                                }
                                value = argv[i];
                            }
                        }
                        // Handle separator (multiple values)
                        if (item.separator) {
                            if (!item.value) {
                                item.value = [];
                            }
                            if (item.commas && typeof value === 'string' && value.includes(',')) {
                                item.value.push(...value.split(','));
                            }
                            else {
                                item.value.push(value);
                            }
                        }
                        else {
                            item.value = value;
                        }
                    }
                }
                else {
                    // Non-option argument
                    this.rest.push(arg);
                }
            }
            // Validate all option values against their ranges
            this.validate();
        }
        catch (e) {
            if (this.template.onerror === 'throw') {
                throw e;
            }
            if (!this.template.silent) {
                App.log.error(String(e));
            }
            if (this.template.usage) {
                this.template.usage();
            }
            if (this.template.onerror === 'exit') {
                App.exit(1);
            }
        }
    }
    /**
     * Clone the template to avoid modifying the original
     */
    cloneTemplate(template) {
        const cloned = {
            options: {},
            usage: template.usage,
            onerror: template.onerror,
            silent: template.silent,
            unknown: template.unknown
        };
        for (const [key, item] of Object.entries(template.options)) {
            cloned.options[key] = { ...item };
        }
        return cloned;
    }
    /**
     * Validate options against the range of permissible values
     */
    validate() {
        for (const [key, item] of Object.entries(this.template.options)) {
            if (item.value === undefined) {
                continue;
            }
            if (item.separator === Array) {
                // Multiple values stored as array
                this.options[key] = [];
                for (const value of item.value) {
                    const v = this.validateItem(key, item, value);
                    this.options[key].push(v);
                }
            }
            else if (typeof item.separator === 'string') {
                // Multiple values concatenated with separator
                this.options[key] = '';
                for (const value of item.value) {
                    if (this.options[key]) {
                        this.options[key] += item.separator + this.validateItem(key, item, value);
                    }
                    else {
                        this.options[key] += this.validateItem(key, item, value);
                    }
                }
            }
            else {
                // Single value
                this.options[key] = this.validateItem(key, item, item.value);
            }
        }
    }
    /**
     * Validate a single option value against its range
     */
    validateItem(key, item, value) {
        if (item.range === Number) {
            if (value) {
                if (typeof value !== 'number') {
                    if (typeof value === 'string' && /^\d+$/.test(value)) {
                        value = Number(value);
                    }
                    else {
                        throw new Error(`Option "${key}" must be a number`);
                    }
                }
            }
            else {
                value = 0;
            }
        }
        else if (item.range === Boolean) {
            if (typeof value === 'boolean') {
                value = value.toString();
            }
            else {
                value = value.toString().toLowerCase();
            }
            if (value !== 'true' && value !== 'false') {
                throw new Error(`Option "${key}" must be true or false`);
            }
            value = value === 'true';
        }
        else if (item.range === String) {
            value = value.toString();
        }
        else if (item.range === Path) {
            value = new Path(value);
        }
        else if (item.range instanceof RegExp) {
            value = value.toString();
            if (!item.range.test(value)) {
                throw new Error(`Option "${key}" has bad value: "${value}"`);
            }
        }
        else if (Array.isArray(item.range)) {
            // Array of permissible values
            let ok = false;
            for (const v of item.range) {
                if (value === v) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                throw new Error(`Option "${key}" has bad value: "${value}"`);
            }
        }
        else {
            // Single permissible value
            if (item.range && value !== item.range) {
                throw new Error(`Option "${key}" has bad value: "${value}"`);
            }
        }
        return value;
    }
}
//# sourceMappingURL=Args.js.map