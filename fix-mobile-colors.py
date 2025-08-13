import re

# Read the HTML file
with open('netlify-deploy/pebble-static-config.html', 'r') as f:
    content = f.read()

# Pattern to match color-option divs with old inline styles
pattern = r'<div class="color-option" data-color="([^"]+)" data-name="([^"]+)" style="background: ([^;]+);[^"]*" onclick="selectColor\(\'([^\']+)\', \'([^\']+)\', ([^)]+)\)" title="([^"]+)"></div>'

# Replacement pattern with only background color
replacement = r'<div class="color-option" data-color="\1" data-name="\2" style="background: \3;" onclick="selectColor(\'\4\', \'\5\', \6)" title="\7"></div>'

# Apply the replacement
new_content = re.sub(pattern, replacement, content)

# Write back to file
with open('netlify-deploy/pebble-static-config.html', 'w') as f:
    f.write(new_content)

print("✅ Mobile color picker optimization complete!")
