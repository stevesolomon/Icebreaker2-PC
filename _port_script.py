import os, re, glob

src_dir = 'src'

replacement = '#include "platform/platform.h"'

# Standard 3DO system includes to remove
std_3do_includes = [
    '#include "graphics.h"',
    '#include "stdio.h"',
    '#include "stdlib.h"',
    '#include "mem.h"',
    '#include "types.h"',
    '#include "hardware.h"',
    '#include "event.h"',
    '#include "strings.h"',
    '#include "access.h"',
    '#include "UMemory.h"',
    '#include "Form3DO.h"',
    '#include "Init3DO.h"',
    '#include "Parse3DO.h"',
    '#include "Utils3DO.h"',
    '#include "audio.h"',
    '#include "music.h"',
    '#include "debug.h"',
    '#include "Debug3DO.h"',
    '#include <CPlusSwiHack.h>',
    '#include "CPlusSwiHack.h"',
    '#include "portfolio.h"',
    '#include "StdLib.h"',
    '#include "Kernel.h"',
    '#include "task.h"',
    '#include <TextLib.h>',
]

files_modified = []

for cpp_file in glob.glob(os.path.join(src_dir, '*.cpp')) + glob.glob(os.path.join(src_dir, '*.h')):
    basename = os.path.basename(cpp_file)
    # Skip icebreaker.h for now - it's the main constants header, mostly portable
    
    with open(cpp_file, 'r', encoding='latin-1') as f:
        content = f.read()
    
    original = content
    
    # Remove individual 3DO include lines
    for inc in std_3do_includes:
        content = content.replace(inc, '')
    
    # Remove comment blocks: "regular includes", "includes (make sure...", "special c++ include"
    content = re.sub(r'/\*{3,}\s*regular includes.*?\*{3,}/', '', content, flags=re.DOTALL|re.IGNORECASE)
    content = re.sub(r'/\*{3,}\s*includes\s*\(make sure.*?\*{3,}/', '', content, flags=re.DOTALL|re.IGNORECASE)
    content = re.sub(r'/\*{3,}\s*special c\+\+.*?\*{3,}/', '', content, flags=re.DOTALL|re.IGNORECASE)
    
    # Add platform include after the header comment if not already there
    if replacement not in content and any(inc in original for inc in std_3do_includes):
        # Find the "magnet includes" or "Magnet includes" comment
        magnet_match = re.search(r'/\*{3,}\s*[Mm]agnet\s+includes\s*\*{3,}/', content)
        if magnet_match:
            pos = magnet_match.start()
            content = content[:pos] + replacement + '\n\n' + content[pos:]
        else:
            # Find first remaining #include
            inc_match = re.search(r'#include\s', content)
            if inc_match:
                pos = inc_match.start()
                content = content[:pos] + replacement + '\n' + content[pos:]
            else:
                # Just add after the first comment block
                end_comment = content.find('*/')
                if end_comment > 0:
                    pos = end_comment + 2
                    content = content[:pos] + '\n\n' + replacement + '\n' + content[pos:]
    
    # Clean up excessive blank lines
    content = re.sub(r'\n{4,}', '\n\n\n', content)
    
    if content != original:
        with open(cpp_file, 'w', encoding='latin-1') as f:
            f.write(content)
        files_modified.append(basename)

print(f'Modified {len(files_modified)} files:')
for f in sorted(files_modified):
    print(f'  {f}')
