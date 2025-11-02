/**
 * Stream - Base stream interface
 *
 * Provides the foundation for all stream-based I/O
 * @spec ejs
 * @stability evolving
 */
/**
 * Stream direction constants
 */
export var StreamDirection;
(function (StreamDirection) {
    StreamDirection[StreamDirection["READ"] = 1] = "READ";
    StreamDirection[StreamDirection["WRITE"] = 2] = "WRITE";
    StreamDirection[StreamDirection["BOTH"] = 3] = "BOTH";
})(StreamDirection || (StreamDirection = {}));
/**
 * Base Stream interface
 * All I/O classes implement this interface
 */
export class Stream {
    /**
     * Read direction constant
     */
    static READ = StreamDirection.READ;
    /**
     * Write direction constant
     */
    static WRITE = StreamDirection.WRITE;
    /**
     * Both directions constant
     */
    static BOTH = StreamDirection.BOTH;
}
//# sourceMappingURL=Stream.js.map