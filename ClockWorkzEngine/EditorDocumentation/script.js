document.addEventListener('DOMContentLoaded', () => {
    // Syntax highlighting logic (Very basic for C++)
    const codeBlocks = document.querySelectorAll('code');
    
    codeBlocks.forEach(block => {
        let html = block.innerHTML;
        // Simple regex to colorize C++ keywords and types
        const keywords = ['class', 'struct', 'public', 'private', 'protected', 'virtual', 'override', 'final', 'void', 'int', 'float', 'double', 'bool', 'auto', 'const', 'std', 'namespace'];
        const types = ['vec2', 'vec3', 'vec4', 'glm::vec2', 'glm::vec4', 'std::string', 'std::vector', 'ECS::Entity', 'ECS::World', 'std::unique_ptr'];
        
        keywords.forEach(keyword => {
            const regex = new RegExp(`\\b${keyword}\\b`, 'g');
            html = html.replace(regex, `<span style="color: #ff7b72;">${keyword}</span>`);
        });
        
        types.forEach(type => {
            // Need to escape special characters for regex
            const escapedType = type.replace(/::/g, '\\::');
            const regex = new RegExp(`\\b${escapedType}\\b`, 'g');
            html = html.replace(regex, `<span style="color: #d2a8ff;">${type}</span>`);
        });

        // Highlight strings
        html = html.replace(/(".*?")/g, `<span style="color: #a5d6ff;">$1</span>`);

        // Highlight comments
        html = html.replace(/(\/\/.*)/g, `<span style="color: #8b949e; font-style: italic;">$1</span>`);

        block.innerHTML = html;
    });

    // Add Copy Buttons to all pre blocks
    const preElements = document.querySelectorAll('pre');
    preElements.forEach((pre, index) => {
        const button = document.createElement('button');
        button.className = 'copy-btn';
        button.innerText = 'Copy';
        button.onclick = () => {
            const code = pre.querySelector('code').innerText;
            navigator.clipboard.writeText(code).then(() => {
                button.innerText = 'Copied!';
                button.style.borderColor = '#58a6ff';
                button.style.color = '#58a6ff';
                setTimeout(() => {
                    button.innerText = 'Copy';
                    button.style.borderColor = 'transparent';
                    button.style.color = 'var(--text-secondary)';
                }, 2000);
            });
        };
        pre.appendChild(button);
    });

    // Intersection Observer for highlighting sidebar links
    const sections = document.querySelectorAll('section[id]');
    const navLinks = document.querySelectorAll('.nav-links li a');

    const observerOptions = {
        root: null,
        rootMargin: '0px',
        threshold: 0.5 // Trigger when 50% of the section is visible
    };

    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                const currentId = entry.target.getAttribute('id');
                navLinks.forEach(link => {
                    link.classList.remove('active');
                    if (link.getAttribute('href') === `#${currentId}`) {
                        link.classList.add('active');
                    }
                });
            }
        });
    }, observerOptions);

    sections.forEach(section => {
        observer.observe(section);
    });

    // Smooth scroll offset for fixed header if added later
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href').substring(1);
            const targetSection = document.getElementById(targetId);
            window.scrollTo({
                top: targetSection.offsetTop - 20,
                behavior: 'smooth'
            });
        });
    });
});
