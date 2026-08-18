const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');

function compileWeb4YamlToHtml(yamlFilePath, outputPath) {
  const fileContent = fs.readFileSync(yamlFilePath, 'utf8');
  const doc = yaml.load(fileContent);

  const formats = {};
  const dataSources = {};
  const extensions = { headlink: [] };

  // 1. Process Formats (^)
  for (const [key, value] of Object.entries(doc)) {
    if (key.startsWith('^')) {
      const formatName = key.slice(1);
      formats[formatName] = value;
    } else if (key.startsWith('$')) {
      const sourceName = key.slice(1);
      if (value.script === 'javascript' && value.code) {
        let output = {};
        const print = (data) => { output = JSON.parse(data); };
        eval(value.code);
        dataSources[sourceName] = output;
      }
    } else if (key.startsWith('+')) {
      const extName = key.slice(1);
      extensions[extName] = value;
    }
  }

  // 2. Resolve AST Component
  function resolveNode(node) {
    if (typeof node === 'string') return node;
    if (Array.isArray(node)) return node.map(resolveNode).join('');

    const [compName, compBody] = Object.entries(node)[0];

    if (compName === 'a') {
      const attrs = compBody;
      return `<a href="${attrs.href}" class="${attrs.class || ''}">${attrs.children}</a>`;
    }

    if (formats[compName]) {
      const fmt = formats[compName];
      let params = { ...fmt.params, ...(compBody.params || {}) };

      // Resolve $ data sources
      for (const [pKey, pVal] of Object.entries(params)) {
        if (typeof pVal === 'string' && pVal.startsWith('$')) {
          const pathParts = pVal.slice(1).split('.');
          const src = dataSources[pathParts[0]];
          params[pKey] = pathParts.length > 1 ? src[pathParts[1]] : src;
        }
      }

      let childrenHtml = '';
      if (compBody.children) {
        childrenHtml = compBody.children.map(resolveNode).join('');
      }

      let template = fmt.template;
      for (const [pKey, pVal] of Object.entries(params)) {
        template = template.replaceAll(`\${${pKey}}`, pVal);
      }
      template = template.replaceAll('${children}', childrenHtml);
      return template;
    }

    const tag = compName;
    const classAttr = compBody.params && compBody.params.class ? ` class="${compBody.params.class}"` : '';
    const idAttr = compBody.params && compBody.params.id ? ` id="${compBody.params.id}"` : '';
    const childrenHtml = compBody.children ? compBody.children.map(resolveNode).join('') : '';

    return `<${tag}${idAttr}${classAttr}>${childrenHtml}</${tag}>`;
  }

  const mainHtml = doc.main.map(resolveNode).join('\n');

  // 3. Assemble Full HTML Document
  const headLinks = (extensions.headlink || [])
    .map(link => `<link rel="${link.rel}" href="${link.href}">`)
    .join('\n  ');

  const fullHtml = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Kubucoin (KUBU) - Native @Username Layer 1 Blockchain</title>
  ${headLinks}
</head>
<body>
${mainHtml}
  <script src="/assets/js/wallet-engine.js"></script>
</body>
</html>`;

  fs.writeFileSync(outputPath, fullHtml, 'utf8');
  console.log(`[Web4 Compiler] Successfully built ${outputPath}`);
}

// Execute Compilation
compileWeb4YamlToHtml(
  path.join(__dirname, 'www/kubucoin.org/html.yaml'),
  path.join(__dirname, 'dist/index.html')
);
