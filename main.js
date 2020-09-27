// Limboole Web GUI

window.input__ = (c) => alert("UNHANDLED INPUT");
window.stderr__ = (c) => alert("UNHANDLED STDERR");
window.stdout__ = (c) => alert("UNHANDLED STDOUT");

class StdinToStdoutProcessor {
    print_line_stdout(line) {
        console.out("STDOUT: " + line);
    };
    print_line_stderr(line) {
        console.out("STDERR: " + line);
    };

    stdin() {
        if (this.input_str_pos < this.input_str.length) {
            let c = this.input_str.charCodeAt(this.input_str_pos);
            this.input_str_pos += 1;
            return c;
        } else {
            return null;
        }
    }
    stdout(code) {
        if (code === "\n".charCodeAt(0) && this.stdout_buf !== "") {
            this.print_line_stdout(this.stdout_buf + "\n");
            this.stdout_buf = "";
        } else {
            this.stdout_buf += String.fromCharCode(code);
        }
    }
    stderr(code) {
        if (code === "\n".charCodeAt(0) && this.stderr_buf !== "") {
            this.print_line_stderr(this.stderr_buf + "\n");
            this.stderr_buf = "";
        } else {
            this.stderr_buf += String.fromCharCode(code);
        }
    }
    
    constructor(creatorFunc) {
        this.input_str_pos = 0;
        this.input_str = "";

        this.stdout_buf = "";
        this.stderr_buf = "";

        let options = {
            preRun: function(mod) {
                function stdin() {
                    return window.input__();
                }

                function stdout(c) {
                    window.stdout__(c);
                }

                function stderr(c) {
                    window.stderr__(c);
                }

                mod.FS.init(stdin, stdout, stderr);
            }
        };

        var self = this;

        console.debug("Creating Processor");
        createLimbooleModule(options).then(function(Module) {
            self.Module = Module;
            window.input__ = function() {return '';};
            window.stdout__ = function(_) {};
            window.stderr__ = function(_) {};

            console.debug("Initial Processor Startup");
            Module.callMain();
            console.debug("Initialized Processor");
            self.limboole = Module.cwrap('limboole', 'number', ['number', 'array', 'number', 'string', 'number']);
        });
    };

    run(input, satcheck, stdout_writeln, stderr_writeln) {
        this.input_str = input;
        this.input_str_pos = 0;
        this.print_line_stdout = stdout_writeln;
        this.print_line_stderr = stderr_writeln;

        window.stdout__ = this.stdout.bind(this);
        window.stderr__ = this.stderr.bind(this);
        
        let status = this.limboole(1, [""], satcheck, input, input.length);
        console.log(status);
        
        if(this.stdout_buf != "") {
            this.print_line_stdout(this.stdout_buf);
            this.stdout_buf = "";
        }
        if(this.stderr_buf != "") {
            this.print_line_stderr(this.stdout_buf);
            this.stderr_buf = "";
        }
    }
};

class ProcessorWrapper {
    constructor(processor, name, args) {
        this.processor = processor;
        this.name = name;
        this.args = args;
    }

    run(input, stdout, stderr) {
        this.processor.run(input, this.args, stdout, stderr);
    }
};


function run_wrapper(wrapper) {
    window.input_textarea = document.getElementById("input");
    window.stdout_textarea = document.getElementById("stdout");
    window.stderr_textarea = document.getElementById("stderr");

    function writeln(element, line) {
        element.innerHTML += line;
    };

    window.stdout_textarea.innerHTML = "";
    window.stderr_textarea.innerHTML = "";

    wrapper.run.bind(wrapper)(window.input_textarea.value, function(line) { writeln(window.stdout_textarea, line); }, function(line) { writeln(window.stderr_textarea, line); } );
}

window.Processors = [
    new StdinToStdoutProcessor(createLimbooleModule),
];

window.Wrappers = [
    new ProcessorWrapper(window.Processors[0], "Validity Check", 0 ),
    new ProcessorWrapper(window.Processors[0], "Satisfiability Check", 1)
];

let selector = document.getElementById("select_wrapper");
for(let i = 0; i < window.Wrappers.length; ++i) {
    let proc = window.Wrappers[i];
    let o = document.createElement('option');
    o.appendChild(document.createTextNode(proc.name));
    o.value = i;
    selector.appendChild(o);
}

window.run_ = function() {
    let selector = document.getElementById("select_wrapper");
    let wr = window.Wrappers[selector.options.selectedIndex];
    run_wrapper(wr);
};
