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
    
    constructor(name, args) {
        this.name = name;
        this.args = args;

        this.input_str_pos = 0;
        this.input_str = "";

        this.stdout_buf = "";
        this.stderr_buf = "";
    };

    run(input, Module, stdout_writeln, stderr_writeln) {
        this.input_str = input;
        this.input_str_pos = 0;
        this.print_line_stdout = stdout_writeln;
        this.print_line_stderr = stderr_writeln;

        window.input__ = this.stdin.bind(this);
        window.stdout__ = this.stdout.bind(this);
        window.stderr__ = this.stderr.bind(this);
        
        Module.callMain(this.args);

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


function run_processor(processor, Module) {
    window.input_textarea = document.getElementById("input");
    window.stdout_textarea = document.getElementById("stdout");
    window.stderr_textarea = document.getElementById("stderr");

    function writeln(element, line) {
        element.innerHTML += line;
    };

    window.stdout_textarea.innerHTML = "";
    window.stderr_textarea.innerHTML = "";

    processor.run.bind(processor)(window.input_textarea.value, Module, function(line) { writeln(window.stdout_textarea, line); }, function(line) { writeln(window.stderr_textarea, line); } );
}

window.Processors = [
    new StdinToStdoutProcessor("Validity Check", "", ),
    new StdinToStdoutProcessor("Satisfiability Check", ["-s"])
];

let selector = document.getElementById("select_processor");
for(let i = 0; i < window.Processors.length; ++i) {
    let proc = window.Processors[i];
    let o = document.createElement('option');
    o.appendChild(document.createTextNode(proc.name));
    o.value = i;
    selector.appendChild(o);
}

window.run_ = function() {
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

    console.debug("Creating...");
    createLimbooleModule(options).then(function(Module) {
        console.debug("Running...");
        let selector = document.getElementById("select_processor");
        let proc = window.Processors[selector.options.selectedIndex];
        run_processor(proc, Module);
        console.debug("Done!");
    });
};
