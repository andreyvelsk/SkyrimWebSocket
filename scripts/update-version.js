#!/usr/bin/env node

/**
 * Script to update version in CMakeLists.txt
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

try {
  let content = fs.readFileSync(cmakelists, 'utf8');
  
  // Find and update VERSION in project() call
  content = content.replace(
    /project\(SkyrimWebSocket VERSION \d+\.\d+\.\d+/,
    `project(SkyrimWebSocket VERSION ${newVersion}`
  );
  
  fs.writeFileSync(cmakelists, content, 'utf8');
  console.log(`✓ Updated CMakeLists.txt to version ${newVersion}`);
} catch (error) {
  console.error('Error updating CMakeLists.txt:', error.message);
  process.exit(1);
}
