// BBQ Controller - Service Worker
// Cache-first for app shell, network-first for data/WebSocket

var CACHE_VERSION = 'bbq-v1';
var APP_SHELL = [
  '/',
  '/index.html',
  '/app.js',
  '/style.css',
  'https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.iife.min.js',
  'https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.min.css'
];

// Install: pre-cache app shell
self.addEventListener('install', function (event) {
  event.waitUntil(
    caches.open(CACHE_VERSION).then(function (cache) {
      return cache.addAll(APP_SHELL);
    }).then(function () {
      return self.skipWaiting();
    })
  );
});

// Activate: clean up old caches
self.addEventListener('activate', function (event) {
  event.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(
        keys.filter(function (key) {
          return key !== CACHE_VERSION;
        }).map(function (key) {
          return caches.delete(key);
        })
      );
    }).then(function () {
      return self.clients.claim();
    })
  );
});

// Fetch: cache-first for app shell, network-first for everything else
self.addEventListener('fetch', function (event) {
  var url = new URL(event.request.url);

  // Skip WebSocket requests (they won't hit fetch, but guard anyway)
  if (url.protocol === 'ws:' || url.protocol === 'wss:') {
    return;
  }

  // Skip non-GET requests
  if (event.request.method !== 'GET') {
    return;
  }

  // Check if this is an app shell resource
  var isAppShell = APP_SHELL.some(function (shellUrl) {
    if (shellUrl.startsWith('http')) {
      return event.request.url === shellUrl;
    }
    return url.pathname === shellUrl || (shellUrl === '/' && url.pathname === '/index.html');
  });

  if (isAppShell) {
    // Cache-first strategy for app shell
    event.respondWith(
      caches.match(event.request).then(function (cached) {
        if (cached) {
          // Return cached, but also update cache in background
          fetch(event.request).then(function (response) {
            if (response && response.status === 200) {
              caches.open(CACHE_VERSION).then(function (cache) {
                cache.put(event.request, response);
              });
            }
          }).catch(function () {
            // Network failed, that's fine - we served from cache
          });
          return cached;
        }
        // Not in cache, fetch from network
        return fetch(event.request).then(function (response) {
          if (response && response.status === 200) {
            var responseClone = response.clone();
            caches.open(CACHE_VERSION).then(function (cache) {
              cache.put(event.request, responseClone);
            });
          }
          return response;
        });
      })
    );
  } else {
    // Network-first for API calls and other resources
    event.respondWith(
      fetch(event.request).catch(function () {
        return caches.match(event.request);
      })
    );
  }
});
