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
}); 