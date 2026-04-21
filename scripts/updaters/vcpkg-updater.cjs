module.exports = {
  readVersion(contents) {
    const pkg = JSON.parse(contents);
    return pkg['version-string'];
  },
  writeVersion(contents, version) {
    const pkg = JSON.parse(contents);
    pkg['version-string'] = version;
    return JSON.stringify(pkg, null, 4) + '\n';
  }
};
