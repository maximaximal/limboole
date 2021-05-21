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
    
    constructor(creatorFunc, resolve, reject) {
        this.input_str_pos = 0;
        this.input_str = "";
        this.ready = false;

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
            self.limboole = Module.cwrap('limboole_extended', 'number', ['number', 'array', 'number', 'string', 'number']);
            resolve();
            self.ready = true;
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
        if(!this.ready()) {
            alert("Not yet ready for execution! Wait until Limboole has been downloaded and compiled!");
            return;
        }
        this.processor.run(input, this.args, stdout, stderr);
    }

    ready() {
        return this.processor.ready;
    }
};


function run_wrapper(wrapper) {
    window.input_textarea = document.getElementById("input");
    window.stdout_textarea = document.getElementById("stdout");
    window.stderr_textarea = document.getElementById("stderr");

    function writeln(element, line) {
        element.value += line;

        element.style.height = 'auto';
        element.style.height = (element.scrollHeight) + 'px';
    };

    window.stdout_textarea.value = "";
    window.stderr_textarea.value = "";

    wrapper.run.bind(wrapper)(window.input_textarea.value, function(line) { writeln(window.stdout_textarea, line); }, function(line) { writeln(window.stderr_textarea, line); } );
}

window.LimbooleLoadedPromise = new Promise(function(resolve, reject) {
    window.Processors = [
        new StdinToStdoutProcessor(createLimbooleModule, resolve, reject),
    ];
});

window.LimbooleLoadedPromise.then(function() {
    $("#loading-indicator").hide();
    $("#run-btn").removeClass("invisible");
});

window.Wrappers = [
    new ProcessorWrapper(window.Processors[0], "Validity Check", 0 ),
    new ProcessorWrapper(window.Processors[0], "Satisfiability Check", 1),
    //new ProcessorWrapper(window.Processors[0], "QBF Validity Check", 2),
    new ProcessorWrapper(window.Processors[0], "QBF Satisfiability Check", 3)
];

let selector = document.getElementById("select_wrapper");
for(let i = 0; i < window.Wrappers.length; ++i) {
    let proc = window.Wrappers[i];
    let o = document.createElement('option');
    o.appendChild(document.createTextNode(proc.name));
    o.value = i;
    selector.appendChild(o);
}

function stateToLocationHash() {
    let selector = document.getElementById("select_wrapper");
    let input = document.getElementById("input");
    return encodeURIComponent(selector.options.selectedIndex + input.value);
}

function applyFromLocationHash() {
    if(window.location.hash != "" && window.location.hash != undefined && window.location.hash != null) {
        let selector = document.getElementById("select_wrapper");
        let input = document.getElementById("input");

        let h = decodeURIComponent(window.location.hash);

        let v = h.charAt(1);
        if(v > 2) { v = 2; }
        selector.value = v;
        input.value = h.substring(2);

        input.style.height = 'auto';
        input.style.height = (input.scrollHeight) + 'px';

        window.LimbooleLoadedPromise.then(function() {
            window.run_();
        });
    }
}

window.run_ = function() {
    let selector = document.getElementById("select_wrapper");
    let wr = window.Wrappers[selector.options.selectedIndex];
    run_wrapper(wr);
    window.location.hash = stateToLocationHash();
};

document.getElementById("input").onkeydown = function(e) {
    if (e.keyCode == 13)
    {
        //      if (e.shiftKey === true)
        if (e.shiftKey)  // thruthy
        {
            window.run_();
            e.preventDefault();
            return false;
        }
        return true;
    }
    return true;
};

let inputDiv = document.getElementById("input");
let inputDivHeaderStatus = document.getElementById("input_annotation");

// File Reader taken from https://stackoverflow.com/a/11313902
if (typeof window.FileReader === 'undefined') {
    // No registering, as file reading is not possible!
} else {
    inputDivHeaderStatus.classList.remove("hide");
    inputDivHeaderStatus.innerHTML = 'Drag&Drop ✓';

    inputDiv.ondragover = function() {
        this.classList.add('hover');
        return false;
    };
    var endevcb = function() {
        this.classList.remove('hover');
        return false;
    }
    inputDiv.ondragend = endevcb;
    inputDiv.onmouseleave = endevcb;

    inputDiv.ondrop = function(e) {
        this.classList.remove('hover');
        e.preventDefault();

        var file = e.dataTransfer.files[0],
            reader = new FileReader();
        reader.onload = function(event) {
            inputDiv.value = event.target.result;
            $(inputDiv).trigger('change');
            window.run_();
        };
        reader.readAsText(file);

        return false;
    };
}

$('textarea').each(function () {
    this.setAttribute('style', 'height:' + (this.scrollHeight) + 'px;overflow-y:hidden;');
}).on('input change', function () {
    this.style.height = 'auto';
    this.style.height = (this.scrollHeight) + 'px';
});



applyFromLocationHash();
