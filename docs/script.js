// Smooth scrolling for navigation links
document.addEventListener('DOMContentLoaded', function() {
    // Handle collapsible sidebar sections
    const sectionToggles = document.querySelectorAll('.nav-section-toggle');
    
    sectionToggles.forEach(toggle => {
        toggle.addEventListener('click', function() {
            const content = this.nextElementSibling;
            const icon = this.querySelector('.toggle-icon');
            const isActive = this.classList.contains('active');
            
            if (isActive) {
                // Collapse this section
                this.classList.remove('active');
                this.setAttribute('aria-expanded', 'false');
                content.classList.add('collapsed');
                icon.textContent = '▶';
            } else {
                // Expand this section and collapse others
                sectionToggles.forEach(otherToggle => {
                    if (otherToggle !== this) {
                        otherToggle.classList.remove('active');
                        otherToggle.setAttribute('aria-expanded', 'false');
                        otherToggle.nextElementSibling.classList.add('collapsed');
                        otherToggle.querySelector('.toggle-icon').textContent = '▶';
                    }
                });
                
                this.classList.add('active');
                this.setAttribute('aria-expanded', 'true');
                content.classList.remove('collapsed');
                icon.textContent = '▼';
            }
        });
    });

    // Handle navigation link clicks
    const navLinks = document.querySelectorAll('.nav-link[href^="#"], .nav-section-content a[href^="#"]');
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href').substring(1);
            const targetElement = document.getElementById(targetId);
            
            if (targetElement) {
                const navbarHeight = document.querySelector('.navbar').offsetHeight;
                const targetPosition = targetElement.offsetTop - navbarHeight - 20;
                
                window.scrollTo({
                    top: targetPosition,
                    behavior: 'smooth'
                });
            }
        });
    });
    
    // Add active state to navigation links based on scroll position
    const sections = document.querySelectorAll('section[id]');
    const navbarHeight = document.querySelector('.navbar').offsetHeight;
    
    function updateActiveNavLink() {
        let current = '';
        
        sections.forEach(section => {
            const sectionTop = section.offsetTop - navbarHeight - 100;
            const sectionHeight = section.offsetHeight;
            
            if (window.scrollY >= sectionTop && window.scrollY < sectionTop + sectionHeight) {
                current = section.getAttribute('id');
            }
        });
        
        // Update active state for sidebar links
        const sidebarLinks = document.querySelectorAll('.nav-section-content a');
        sidebarLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === `#${current}`) {
                link.classList.add('active');
                
                // Auto-expand the section containing the active link
                const parentSection = link.closest('.nav-section');
                if (parentSection) {
                    const toggle = parentSection.querySelector('.nav-section-toggle');
                    const content = parentSection.querySelector('.nav-section-content');
                    
                    if (!toggle.classList.contains('active')) {
                        // Collapse other sections
                        sectionToggles.forEach(otherToggle => {
                            if (otherToggle !== toggle) {
                                otherToggle.classList.remove('active');
                                otherToggle.setAttribute('aria-expanded', 'false');
                                otherToggle.nextElementSibling.classList.add('collapsed');
                                otherToggle.querySelector('.toggle-icon').textContent = '▶';
                            }
                        });
                        
                        // Expand current section
                        toggle.classList.add('active');
                        toggle.setAttribute('aria-expanded', 'true');
                        content.classList.remove('collapsed');
                        toggle.querySelector('.toggle-icon').textContent = '▼';
                    }
                }
            }
        });
        
        // Also update main nav links
        const mainNavLinks = document.querySelectorAll('.nav-link[href^="#"]');
        mainNavLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === `#${current}`) {
                link.classList.add('active');
            }
        });
    }
    
    // Update active nav link on scroll
    window.addEventListener('scroll', updateActiveNavLink);
    updateActiveNavLink(); // Initial call
    
    // Add copy functionality to code blocks
    const codeBlocks = document.querySelectorAll('pre code');
    
    codeBlocks.forEach(block => {
        const pre = block.parentElement;
        const copyButton = document.createElement('button');
        copyButton.className = 'copy-button';
        copyButton.innerHTML = '📋';
        copyButton.title = 'Copy code';
        
        copyButton.addEventListener('click', async () => {
            try {
                await navigator.clipboard.writeText(block.textContent);
                copyButton.innerHTML = '✅';
                copyButton.title = 'Copied!';
                
                setTimeout(() => {
                    copyButton.innerHTML = '📋';
                    copyButton.title = 'Copy code';
                }, 2000);
            } catch (err) {
                console.error('Failed to copy text: ', err);
            }
        });
        
        pre.style.position = 'relative';
        pre.appendChild(copyButton);
    });
    
    // Add fade-in animation for feature cards
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -50px 0px'
    };
    
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
            }
        });
    }, observerOptions);
    
    // Observe feature cards and doc cards
    const animatedElements = document.querySelectorAll('.feature-card, .doc-card, .example-card');
    animatedElements.forEach(el => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(20px)';
        el.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
        observer.observe(el);
    });

    // Initialize WASM Gem interpreter
    initializeGemInterpreter();
    
    // Initialize code editor
    initializeCodeEditor();
});

// WASM Gem Interpreter Integration
let gemModule = null;
let gemInitialized = false;
let capturedOutput = '';

async function initializeGemInterpreter() {
    try {
        // Load the WASM module using the UMD format
        const GemModule = window.GemModule;
        if (!GemModule) {
            throw new Error('GemModule not found. Make sure gem.js is loaded.');
        }
        
        // Configure module to capture output
        const moduleConfig = {
            print: function(text) {
                capturedOutput += text + '\n';
            },
            printErr: function(text) {
                capturedOutput += 'Error: ' + text + '\n';
            }
        };
        
        gemModule = await GemModule(moduleConfig);
        
        // Initialize the Gem VM
        gemModule.ccall('gem_init', null, [], []);
        gemInitialized = true;
        
        // Update the terminal to show ready state
        updateTerminalReady();
        
    } catch (error) {
        console.error('Failed to initialize Gem WASM interpreter:', error);
        showTerminalError('Failed to load Gem interpreter. Please refresh the page.');
    }
}

function updateTerminalReady() {
    const terminal = document.getElementById('terminal-output');
    if (terminal) {
        terminal.innerHTML = `
            <div class="terminal-line">
                <span class="terminal-prompt">$ </span>
                <span class="terminal-command">gemc hello.gem<span class="terminal-cursor">|</span></span>
            </div>
        `;
    }
}

function showTerminalError(message) {
    const terminal = document.getElementById('terminal-output');
    if (terminal) {
        terminal.innerHTML = `
            <div class="terminal-line">
                <span class="terminal-prompt">$ </span>
                <span class="terminal-command">gemc hello.gem</span>
            </div>
            <div class="terminal-error">${message}</div>
            <div class="terminal-line">
                <span class="terminal-prompt">$ </span>
                <span class="terminal-cursor">_</span>
            </div>
        `;
    }
}

// Interactive demo functions
function runGemCode() {
    console.log('Run button clicked!');
    
    if (!gemInitialized || !gemModule) {
        showTerminalError('Gem interpreter not ready. Please wait or refresh the page.');
        return;
    }

    const editor = document.getElementById('gem-editor');
    const terminal = document.getElementById('terminal-output');
    const code = editor.value;
    
    // Clear previous output and show running state
    terminal.innerHTML = `
        <div class="terminal-line">
            <span class="terminal-prompt">$ </span>
            <span class="terminal-command">gemc hello.gem</span>
        </div>
        <div class="terminal-output-text">Running...</div>
    `;
    
    // Simulate execution delay for realism
    setTimeout(() => {
        try {
            // Clear previous output
            capturedOutput = '';
            gemModule.ccall('gem_clear_output', null, [], []);
            
            // Interpret the source code
            const result = gemModule.ccall('gem_interpret', 'number', ['string'], [code]);
            
            let outputHtml = `
                <div class="terminal-line">
                    <span class="terminal-prompt">$ </span>
                    <span class="terminal-command">gemc hello.gem</span>
                </div>
            `;
            
            // Check result codes (from vm.h)
            // INTERPRET_OK = 0, INTERPRET_COMPILE_ERROR = 1, INTERPRET_RUNTIME_ERROR = 2
            if (result === 0) {
                if (capturedOutput && capturedOutput.trim()) {
                    // Split output by newlines and create separate lines for each
                    const outputLines = capturedOutput.trim().split('\n');
                    const formattedOutput = outputLines.map(line => 
                        `<div class="terminal-success">${line}</div>`
                    ).join('');
                    outputHtml += formattedOutput;
                } else {
                    outputHtml += `<div class="terminal-output-text">Program executed successfully!</div>`;
                }
            } else if (result === 1) {
                outputHtml += `<div class="terminal-error">Compile Error</div>`;
                if (capturedOutput && capturedOutput.trim()) {
                    const outputLines = capturedOutput.trim().split('\n');
                    const formattedOutput = outputLines.map(line => 
                        `<div class="terminal-output-text">${line}</div>`
                    ).join('');
                    outputHtml += formattedOutput;
                }
            } else if (result === 2) {
                outputHtml += `<div class="terminal-error">Runtime Error</div>`;
                if (capturedOutput && capturedOutput.trim()) {
                    const outputLines = capturedOutput.trim().split('\n');
                    const formattedOutput = outputLines.map(line => 
                        `<div class="terminal-output-text">${line}</div>`
                    ).join('');
                    outputHtml += formattedOutput;
                }
            } else {
                outputHtml += `<div class="terminal-error">Unknown error (code: ${result})</div>`;
            }
            
            outputHtml += `
                <div class="terminal-line">
                    <span class="terminal-prompt">$ </span>
                    <span class="terminal-cursor">_</span>
                </div>
            `;
            
            terminal.innerHTML = outputHtml;
            terminal.scrollTop = terminal.scrollHeight;
            
        } catch (error) {
            console.error('Error running Gem code:', error);
            showTerminalError(`Execution failed: ${error.message}`);
        }
    }, 500);
}

function clearTerminal() {
    if (gemInitialized && gemModule) {
        capturedOutput = '';
        gemModule.ccall('gem_clear_output', null, [], []);
        updateTerminalReady();
    } else {
        const terminal = document.getElementById('terminal-output');
        terminal.innerHTML = `
            <div class="terminal-line">
                <span class="terminal-prompt">$ </span>
                <span class="terminal-command">gemc hello.gem</span>
            </div>
            <div class="terminal-output-text">Gem interpreter loading...</div>
        `;
    }
}

// Code Editor Enhancement
function initializeCodeEditor() {
    const editor = document.getElementById('gem-editor');
    const lineNumbers = document.getElementById('line-numbers');
    const syntaxHighlight = document.getElementById('syntax-highlight');
    
    if (!editor || !lineNumbers || !syntaxHighlight) return;
    
    function updateEditor() {
        const code = editor.value;
        updateLineNumbers(code);
        updateSyntaxHighlighting(code);
    }
    
    function updateLineNumbers(code) {
        const lines = code.split('\n');
        const lineNumbersText = lines.map((_, index) => (index + 1).toString()).join('\n');
        lineNumbers.textContent = lineNumbersText;
    }
    
    function updateSyntaxHighlighting(code) {
        const syntaxHighlight = document.getElementById('syntax-highlight');
        syntaxHighlight.textContent = code;
        
        // Use Prism.js to highlight the code
        if (window.Prism) {
            Prism.highlightElement(syntaxHighlight);
        }
    }
    
    // Sync scrolling between editor and syntax highlight
    function syncScroll() {
        const syntaxHighlightContainer = syntaxHighlight.parentElement;
        syntaxHighlightContainer.scrollTop = editor.scrollTop;
        syntaxHighlightContainer.scrollLeft = editor.scrollLeft;
    }
    
    // Event listeners
    editor.addEventListener('input', updateEditor);
    editor.addEventListener('scroll', syncScroll);
    
    // Handle tab key for proper indentation
    editor.addEventListener('keydown', function(e) {
        if (e.key === 'Tab') {
            e.preventDefault();
            const start = this.selectionStart;
            const end = this.selectionEnd;
            
            // Insert tab character
            this.value = this.value.substring(0, start) + '    ' + this.value.substring(end);
            
            // Move cursor
            this.selectionStart = this.selectionEnd = start + 4;
            
            // Update highlighting
            updateEditor();
        }
    });
    
    // Initial update
    updateEditor();
} 