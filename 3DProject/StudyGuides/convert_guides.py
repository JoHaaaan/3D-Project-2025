import os
import re

# Directory containing guides
GUIDE_DIR = r"C:\Users\Barnen\Desktop\3D-Project-2025\3DProject\StudyGuides"

# List of guides in order
GUIDE_FILES = [
    "01_study_guide_overall_architecture.md",
    "02_study_guide_obj_parser_buffers.md",
    "03_study_guide_quadtree_culling.md",
    "04_study_guide_deferred_rendering.md",
    "05_study_guide_shadow_mapping.md",
    "06_study_guide_phong_tessellation.md",
    "07_study_guide_normal_parallax.md",
    "08_study_guide_environment_mapping.md",
    "09_study_guide_particle_system.md",
    "10_study_guide_binding_dispatch.md",
    "11_study_guide_master_faq.md",
    "12_mock_presentations_with_teacher.md"
]

# Map file base name to display name
GUIDE_NAMES = {
    "01_study_guide_overall_architecture": "01. Overall Architecture",
    "02_study_guide_obj_parser_buffers": "02. Mesh Loading & Buffers",
    "03_study_guide_quadtree_culling": "03. Quadtree Culling",
    "04_study_guide_deferred_rendering": "04. Deferred Rendering",
    "05_study_guide_shadow_mapping": "05. Shadow Mapping",
    "06_study_guide_phong_tessellation": "06. Phong Tessellation",
    "07_study_guide_normal_parallax": "07. Normal & Parallax Maps",
    "08_study_guide_environment_mapping": "08. Environment Maps",
    "09_study_guide_particle_system": "09. GPU Particles",
    "10_study_guide_binding_dispatch": "10. Bindings & Dispatch",
    "11_study_guide_master_faq": "11. Master FAQ",
    "12_mock_presentations_with_teacher": "12. Mock Presentations"
}

def parse_markdown(md_text):
    # Escape HTML special characters in raw markdown first to prevent browser layout breakage
    md_text = md_text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

    # 1. Convert code blocks with syntax styling (supporting indents and CRLF)
    def replace_code_block(match):
        lang = match.group(1) or 'plaintext'
        code = match.group(2)
        return f'<pre><code class="language-{lang}">{code}</code></pre>'
    
    md_text = re.sub(r'(?m)^[ \t]*```(\w*)\r?\n(.*?)\r?\n[ \t]*```', replace_code_block, md_text, flags=re.DOTALL)
    
    # 2. Convert inline code
    md_text = re.sub(r'`([^`]+)`', r'<code>\1</code>', md_text)
    
    # 3. Protect code blocks and inline code from other markdown parser rules (like bold/italics matching C++ code)
    code_blocks = []
    inline_codes = []
    
    def save_code_block(match):
        code_blocks.append(match.group(0))
        return f'<!--CODEBLOCK_{len(code_blocks)-1}-->'
        
    def save_inline_code(match):
        inline_codes.append(match.group(0))
        return f'<!--INLINECODE_{len(inline_codes)-1}-->'
        
    # Extract code blocks first, then inline codes to prevent incorrect substring matches
    md_text = re.sub(r'<pre><code class="language-\w+">.*?</code></pre>', save_code_block, md_text, flags=re.DOTALL)
    md_text = re.sub(r'<code>.*?</code>', save_inline_code, md_text, flags=re.DOTALL)
    
    # 4. Convert alert blocks (> [!NOTE], > [!TIP], etc.)
    def replace_alert(match):
        alert_type = match.group(1).upper()
        content = match.group(2).strip()
        alert_class = "alert-note"
        icon = "📌"
        if "TIP" in alert_type:
            alert_class = "alert-tip"
            icon = "💡"
        elif "IMPORTANT" in alert_type:
            alert_class = "alert-important"
            icon = "⚠️"
        elif "WARNING" in alert_type:
            alert_class = "alert-warning"
            icon = "⚡"
            
        return f'<div class="alert {alert_class}"><span class="alert-icon">{icon}</span><div class="alert-content">{content}</div></div>'
        
    md_text = re.sub(r'>\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\r?\n>\s*(.*?)(?=\r?\n\r?\n|\r?\n[^>])', replace_alert, md_text, flags=re.DOTALL)

    # Convert standard blockquotes
    md_text = re.sub(r'^>\s+(.*?)$', r'<blockquote>\1</blockquote>', md_text, flags=re.MULTILINE)

    # Convert headers (h1 to h3)
    md_text = re.sub(r'^#\s+(.*?)$', r'<h1>\1</h1>', md_text, flags=re.MULTILINE)
    md_text = re.sub(r'^##\s+(.*?)$', r'<h2>\1</h2>', md_text, flags=re.MULTILINE)
    md_text = re.sub(r'^###\s+(.*?)$', r'<h3>\1</h3>', md_text, flags=re.MULTILINE)

    # Convert lists
    def parse_lists(match):
        items = match.group(0).strip().split('\n')
        list_html = "<ul>\n"
        for item in items:
            clean_item = re.sub(r'^\s*[-*+]\s+', '', item)
            if clean_item.startswith('[x]') or clean_item.startswith('[X]'):
                clean_item = '<input type="checkbox" checked disabled> ' + clean_item[3:]
            elif clean_item.startswith('[ ]'):
                clean_item = '<input type="checkbox" disabled> ' + clean_item[3:]
            list_html += f"  <li>{clean_item}</li>\n"
        list_html += "</ul>"
        return list_html

    md_text = re.sub(r'(?:^\s*[-*+]\s+.*?$(?:\r?\n)?)+', parse_lists, md_text, flags=re.MULTILINE)

    # Convert links
    def replace_links(match):
        text = match.group(1)
        url = match.group(2)
        for name in GUIDE_NAMES.keys():
            if name in url:
                url = url.replace(name + ".md", name + ".html")
        return f'<a href="{url}">{text}</a>'
        
    md_text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', replace_links, md_text)

    # Bold and italics (these run safely on text with placeholders, avoiding C++ asterisks)
    md_text = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', md_text)
    md_text = re.sub(r'\*([^*]+)\*', r'<em>\1</em>', md_text)

    # Convert horizontal rules
    md_text = re.sub(r'^---$', r'<hr>', md_text, flags=re.MULTILINE)

    # Tables
    def replace_table(match):
        lines = match.group(0).strip().split('\n')
        if len(lines) < 2:
            return match.group(0)
        
        headers = [h.strip() for h in lines[0].split('|')[1:-1]]
        html_table = '<table>\n  <thead>\n    <tr>\n'
        for h in headers:
            html_table += f'      <th>{h}</th>\n'
        html_table += '    </tr>\n  </thead>\n  <tbody>\n'
        
        for r in lines[2:]:
            cols = [c.strip() for c in r.split('|')[1:-1]]
            html_table += '    <tr>\n'
            for c in cols:
                html_table += f'      <td>{c}</td>\n'
            html_table += '    </tr>\n'
        html_table += '  </tbody>\n</table>'
        return html_table
        
    md_text = re.sub(r'(?:^\|.*?\|\n)+', replace_table, md_text, flags=re.MULTILINE)

    # Convert double line breaks into paragraphs
    paragraphs = md_text.split('\n\n')
    formatted_pgs = []
    for p in paragraphs:
        p_strip = p.strip()
        if not p_strip:
            continue
        if p_strip.startswith('<h') or p_strip.startswith('<pre') or p_strip.startswith('<ul') or p_strip.startswith('<div') or p_strip.startswith('<hr') or p_strip.startswith('<table') or p_strip.startswith('<blockquote>') or p_strip.startswith('<!--CODEBLOCK_'):
            formatted_pgs.append(p_strip)
        else:
            p_clean = p_strip.replace('\n', ' ')
            formatted_pgs.append(f'<p>{p_clean}</p>')
            
    md_text = '\n'.join(formatted_pgs)
    
    # 5. Restore the placeholders in reverse order
    for i, inline in enumerate(inline_codes):
        md_text = md_text.replace(f'<!--INLINECODE_{i}-->', inline)
    for i, code in enumerate(code_blocks):
        md_text = md_text.replace(f'<!--CODEBLOCK_{i}-->', code)
        
    return md_text

def build_nav_html(current_key):
    nav_html = '<div class="sidebar">\n'
    nav_html += '  <div class="sidebar-header">DV1542 Study Kit</div>\n'
    nav_html += '  <nav class="sidebar-nav">\n'
    for f in GUIDE_FILES:
        base_name = os.path.splitext(f)[0]
        display_name = GUIDE_NAMES.get(base_name, base_name)
        active_class = "active" if base_name == current_key else ""
        nav_html += f'    <a class="nav-item {active_class}" href="{base_name}.html">{display_name}</a>\n'
    nav_html += '  </nav>\n'
    nav_html += '</div>\n'
    return nav_html

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title} - DV1542 Study Kit</title>
    
    <!-- Fonts -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Fira+Code:wght@400;500&family=Inter:wght@300;400;500;600;700&family=Outfit:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    
    <!-- Highlight.js for Code Highlighting -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.7.0/styles/tokyo-night-dark.min.css">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.7.0/highlight.min.js"></script>
    <script>hljs.highlightAll();</script>
    
    <!-- MathJax Configuration -->
    <script>
    window.MathJax = {{
        tex: {{
            inlineMath: [['$', '$'], ['\\\\(', '\\\\)']],
            displayMath: [['$$', '$$'], ['\\\\[', '\\\\]']]
        }}
    }};
    </script>
    <script id="MathJax-script" async src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"></script>
    
    <style>
        :root {{
            --bg-main: #0b0f19;
            --bg-card: #151b26;
            --bg-sidebar: #0f131f;
            --border-color: #242c3d;
            --text-main: #e2e8f0;
            --text-muted: #94a3b8;
            --primary: #6366f1;
            --primary-glow: rgba(99, 102, 241, 0.15);
            --accent: #a855f7;
            --accent-glow: rgba(168, 85, 247, 0.15);
            --success: #10b981;
            --warning: #f59e0b;
        }}

        * {{
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }}

        body {{
            font-family: 'Inter', sans-serif;
            background-color: var(--bg-main);
            color: var(--text-main);
            display: flex;
            min-height: 100vh;
            line-height: 1.6;
        }}

        /* Sidebar Navigation Layout */
        .sidebar {{
            width: 280px;
            background-color: var(--bg-sidebar);
            border-right: 1px solid var(--border-color);
            display: flex;
            flex-direction: column;
            position: fixed;
            top: 0;
            bottom: 0;
            left: 0;
            z-index: 100;
        }}

        .sidebar-header {{
            padding: 24px;
            font-family: 'Outfit', sans-serif;
            font-size: 1.25rem;
            font-weight: 700;
            background: linear-gradient(135deg, var(--primary), var(--accent));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            border-bottom: 1px solid var(--border-color);
        }}

        .sidebar-nav {{
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 6px;
            overflow-y: auto;
            flex: 1;
        }}

        .nav-item {{
            text-decoration: none;
            color: var(--text-muted);
            padding: 10px 14px;
            border-radius: 8px;
            font-size: 0.9rem;
            font-weight: 500;
            transition: all 0.2s ease;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }}

        .nav-item:hover {{
            background-color: rgba(255, 255, 255, 0.03);
            color: var(--text-main);
        }}

        .nav-item.active {{
            background: linear-gradient(90deg, var(--primary-glow), rgba(168, 85, 247, 0.05));
            border-left: 3px solid var(--primary);
            color: #fff;
            padding-left: 11px;
        }}

        /* Main Content Layout */
        .main-content {{
            margin-left: 280px;
            flex: 1;
            padding: 40px 60px;
            max-width: 1000px;
        }}

        /* Typography */
        h1, h2, h3, h4 {{
            font-family: 'Outfit', sans-serif;
            font-weight: 700;
            letter-spacing: -0.02em;
            color: #fff;
        }}

        h1 {{
            font-size: 2.25rem;
            margin-bottom: 24px;
            background: linear-gradient(135deg, #fff 60%, var(--text-muted));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }}

        h2 {{
            font-size: 1.5rem;
            margin-top: 36px;
            margin-bottom: 16px;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 8px;
        }}

        h3 {{
            font-size: 1.15rem;
            margin-top: 24px;
            margin-bottom: 12px;
            color: var(--primary);
        }}

        p {{
            margin-bottom: 18px;
            color: var(--text-muted);
            font-size: 0.975rem;
        }}

        /* Lists */
        ul, ol {{
            margin-bottom: 20px;
            padding-left: 24px;
            color: var(--text-muted);
        }}

        li {{
            margin-bottom: 8px;
            font-size: 0.95rem;
        }}

        li input[type="checkbox"] {{
            margin-right: 8px;
            accent-color: var(--primary);
            transform: scale(1.1);
            vertical-align: middle;
        }}

        /* Code Blocks */
        pre {{
            background-color: var(--bg-card);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 24px;
            overflow-x: auto;
        }}

        code {{
            font-family: 'Fira Code', monospace;
            font-size: 0.875rem;
            background-color: rgba(255, 255, 255, 0.05);
            padding: 2px 6px;
            border-radius: 4px;
            color: var(--primary);
            word-break: break-word;
        }}

        pre code {{
            background-color: transparent;
            padding: 0;
            border-radius: 0;
            color: inherit;
            word-break: normal;
        }}

        /* Links */
        a {{
            color: var(--primary);
            text-decoration: none;
            transition: color 0.15s ease;
        }}

        a:hover {{
            color: var(--accent);
            text-decoration: underline;
        }}

        /* Horizontal Rule */
        hr {{
            border: 0;
            height: 1px;
            background-color: var(--border-color);
            margin: 40px 0;
        }}

        /* Tables */
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 24px;
            border-radius: 8px;
            overflow: hidden;
            border: 1px solid var(--border-color);
        }}

        th, td {{
            padding: 12px 16px;
            text-align: left;
            font-size: 0.9rem;
        }}

        th {{
            background-color: var(--bg-sidebar);
            color: #fff;
            font-weight: 600;
            border-bottom: 1px solid var(--border-color);
        }}

        td {{
            background-color: var(--bg-card);
            color: var(--text-muted);
            border-bottom: 1px solid var(--border-color);
        }}

        tr:last-child td {{
            border-bottom: none;
        }}

        /* Alerts */
        .alert {{
            background-color: var(--bg-card);
            border-radius: 12px;
            padding: 16px 20px;
            margin-bottom: 24px;
            display: flex;
            gap: 16px;
            align-items: flex-start;
            border-left: 4px solid var(--primary);
        }}

        .alert-note {{
            border-left-color: var(--primary);
            background: linear-gradient(90deg, var(--bg-card) 95%, rgba(99, 102, 241, 0.03));
        }}

        .alert-tip {{
            border-left-color: var(--success);
            background: linear-gradient(90deg, var(--bg-card) 95%, rgba(16, 185, 129, 0.03));
        }}

        .alert-important {{
            border-left-color: var(--warning);
            background: linear-gradient(90deg, var(--bg-card) 95%, rgba(245, 158, 11, 0.03));
        }}

        .alert-warning {{
            border-left-color: var(--accent);
            background: linear-gradient(90deg, var(--bg-card) 95%, rgba(168, 85, 247, 0.03));
        }}

        .alert-icon {{
            font-size: 1.3rem;
            line-height: 1;
        }}

        .alert-content {{
            flex: 1;
        }}

        .alert-content p {{
            margin-bottom: 0;
        }}

        blockquote {{
            border-left: 4px solid var(--border-color);
            padding-left: 16px;
            color: var(--text-muted);
            font-style: italic;
            margin-bottom: 20px;
        }}
    </style>
</head>
<body>
    {nav}
    
    <main class="main-content">
        {content}
    </main>
</body>
</html>
"""

def main():
    print("Converting guides from Markdown to beautiful HTML pages...")
    for filename in GUIDE_FILES:
        md_path = os.path.join(GUIDE_DIR, filename)
        if not os.path.exists(md_path):
            print(f"Skipping missing file: {filename}")
            continue
            
        base_name = os.path.splitext(filename)[0]
        title = GUIDE_NAMES.get(base_name, base_name)
        
        with open(md_path, 'r', encoding='utf-8') as f:
            md_text = f.read()
            
        # Parse markdown text to HTML format
        html_content = parse_markdown(md_text)
        
        # Build sidebar navigation
        nav_html = build_nav_html(base_name)
        
        # Build final HTML template
        final_html = HTML_TEMPLATE.format(
            title=title,
            nav=nav_html,
            content=html_content
        )
        
        html_filename = base_name + ".html"
        html_path = os.path.join(GUIDE_DIR, html_filename)
        with open(html_path, 'w', encoding='utf-8') as f:
            f.write(final_html)
            
        print(f"Generated: {html_filename}")
        
    print("All conversions completed successfully!")

if __name__ == '__main__':
    main()
