import type { PluginManifest } from 'ivandaw-core';

// Plain data, so the DAW build can write it out as manifest.json without loading the plugin.
export const ID = 'com.ivandaw.banana-chords';
export const VERSION = '0.0.1'; // same as VERSION in CMakeLists.txt

export const manifest: PluginManifest = {
  id: ID,
  name: 'Banana Chords',
  vendor: 'zhirnovcodes',
  version: VERSION,
  apiVersion: 2,
  kind: 'midi',
  // A pure generator: no note input. CC 120/123 still releases every note if a host sends it.
  ports: [
    { id: 'in', label: 'Control', kind: 'events', direction: 'in', accepts: ['automation', 'tempo', 'transport'] },
    { id: 'out', label: 'Notes', kind: 'events', direction: 'out', accepts: ['midi'] },
  ],
  ui: { width: 560, height: 470, resizable: false },
  features: ['state'],
};
