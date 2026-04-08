#!/usr/bin/env node

/**
 * Script to update version in CMakeLists.txt and vcpkg.json
 * Called by standard-version as a custom updater
 * 
 * Usage: node scripts/update-version.js <newVersion>
 */

import fs from 'fs';
import path from 'path';

const newVersion = process.argv[2];

if (!newVersion) {
  console.error('Usage: update-version.js <newVersion>');
  process.exit(1);
}

const cmakelists = path.join(process.cwd(), 'CMakeLists.txt');
const vcpkgJson = path.join(process.cwd(), 'vcpkg.json');

try {
  // Update CMakeLists.txt
  let cmakelitsContent = fs.readFileSync(cmakelists, 'utf8');
  cmakelitsContent = cmakelitsContent.replace(
    /project\(SkyrimWebSocket VERSION \d+\.\d+\.\d+/,
    `project(SkyrimWebSocket VERSION ${newVersion}`
  );
  fs.writeFileSync(cmakelists, cmakelitsContent, 'utf8');
  console.log(`✓ Updated CMakeLists.txt to version ${newVersion}`);

  // Update vcpkg.json
  let vcpkgContent = fs.readFileSync(vcpkgJson, 'utf8');
  const vcpkg = JSON.parse(vcpkgContent);
  vcpkg['version-string'] = newVersion;
  fs.writeFileSync(vcpkgJson, JSON.stringify(vcpkg, null, 4) + '\n', 'utf8');
  console.log(`✓ Updated vcpkg.json to version ${newVersion}`);
} catch (error) {
  console.error('Error updating files:', error.message);
  process.exit(1);
}
