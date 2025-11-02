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
/**
 * Option specification for a command-line flag
 */
export interface ArgsOptionSpec {
    /** Alias (short form) for the option */
    alias?: string;
    /** Range of permissible values: Type constructor, RegExp, or array of values */
    range?: typeof Number | typeof String | typeof Boolean | typeof Path | RegExp | any[];
    /** Default value for the option */
    value?: any;
    /** Separator for multiple values: string for concatenation, Array for array storage */
    separator?: string | typeof Array;
    /** If true, split comma-separated values */
    commas?: boolean;
}
/**
 * Template for command-line argument parsing
 */
export interface ArgsTemplate {
    /** Object defining each command-line option */
    options: {
        [key: string]: ArgsOptionSpec;
    };
    /** Function to invoke for argument parse errors (usage message) */
    usage?: () => void;
    /** Error handling: 'throw' to throw exceptions, 'exit' to exit the application */
    onerror?: 'throw' | 'exit';
    /** If true, suppresses error messages */
    silent?: boolean;
    /** Callback to invoke for unknown arguments */
    unknown?: (argv: string[], index: number) => number;
}
/**
 * Args - Command-line argument parser
 *
 * Parses command-line arguments according to a template specification
 */
export declare class Args {
    /** Program name from App.args[0] */
    program: Path;
    /** Remaining arguments that follow the command options */
    rest: string[];
    /** Parsed command line options */
    options: {
        [key: string]: any;
    };
    /** Copy of the argument template specification */
    template: ArgsTemplate;
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
    constructor(template: ArgsTemplate, argv?: string[]);
    /**
     * Clone the template to avoid modifying the original
     */
    private cloneTemplate;
    /**
     * Validate options against the range of permissible values
     */
    private validate;
    /**
     * Validate a single option value against its range
     */
    private validateItem;
}
//# sourceMappingURL=Args.d.ts.map