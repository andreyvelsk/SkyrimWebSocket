module.exports = {
  readVersion(contents) {
    const match = contents.match(/project\(SkyrimWebSocket VERSION (\d+\.\d+\.\d+)/);
    return match ? match[1] : null;
  },
  writeVersion(contents, version) {
    return contents.replace(
      /project\(SkyrimWebSocket VERSION \d+\.\d+\.\d+/,
      `project(SkyrimWebSocket VERSION ${version}`
    );
  }
};
