declare global {
  // Third-party libraries
  const MochaUI: any;
  const MUI: any;
  const Asset: any;
  const vanillaSelectBox: any;
  const clipboardCopy: (text: string) => Promise<void>;

  // Global variables used throughout the application
  var torrentsTable: TorrentsTable;
  var updateMainData: () => void;

  // Template string constant
  const CACHEID: string;
  const LANG: string;

  interface QBittorrent {
    AddTorrent?: typeof addTorrentModule;
    Cache?: typeof cacheModule;
    ColorScheme?: typeof colorSchemeModule;
    ContextMenu?: typeof contextMenuModule;
    FileTree?: typeof fileTreeModule;
    Misc?: typeof miscModule;
    [key: string]: any;
  }

  interface Window {
    qBittorrent: QBittorrent;
  }

  interface Element {
    // MooTools
    getSize(): { x: number; y: number };
    isVisible(): boolean;
  }

  interface Document {
    // MooTools
    getSize(): { x: number; y: number };
  }
}

export {};
