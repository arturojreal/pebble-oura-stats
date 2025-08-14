// =============================================================================
// OURA STATS WATCHFACE - JAVASCRIPT COMPONENT (ES5 Compatible)
// =============================================================================
// Oura API configuration
// Handles OAuth2 authentication and API calls to Oura Ring API v2
// Runs on phone, sends data to Pebble watch
// =============================================================================

// Debug logging control
var DEBUG = true; // enable verbose console logs
var WATCH_DEBUG = false; // do not spam watch debug layer unless explicitly enabled
(function(){
  var _log = console.log;
  console.log = function() {
    if (DEBUG) {
      try { _log.apply(console, arguments); } catch (e) { /* older JS env */ _log(arguments && arguments[0]); }
    }
  };
})();

// -----------------------------------------------------------------------------
// AppMessage queue: serialize sends and retry on transient errors
// -----------------------------------------------------------------------------
var MSG_MAX_RETRIES = 3;
var MSG_RETRY_DELAY_MS = 250; // short backoff
var ACTIVITY_SEND_DELAY_MS = 1000; // delay aggregated send slightly
var g_msg_queue = [];
var g_msg_sending = false;

function enqueueMessage(payload, onSuccess, onError) {
  g_msg_queue.push({ payload: payload, tries: 0, onSuccess: onSuccess, onError: onError });
  processMessageQueue();
}

function processMessageQueue() {
  if (g_msg_sending) return;
  if (!g_msg_queue.length) return;
  var item = g_msg_queue[0];
  g_msg_sending = true;
  Pebble.sendAppMessage(item.payload, function() {
    console.log('[queue] sent ok:', item.payload);
    g_msg_sending = false;
    // pop and continue
    g_msg_queue.shift();
    if (item.onSuccess) { try { item.onSuccess(); } catch (e) {} }
    processMessageQueue();
  }, function(err) {
    g_msg_sending = false;
    item.tries += 1;
    console.warn('[queue] send failed (try ' + item.tries + '):', err);
    if (item.tries < MSG_MAX_RETRIES) {
      setTimeout(function(){ processMessageQueue(); }, MSG_RETRY_DELAY_MS);
    } else {
      // give up on this message; drop it to unblock queue
      g_msg_queue.shift();
      if (item.onError) { try { item.onError(err); } catch (e) {} }
      processMessageQueue();
    }
  });
}

// Cache management
var CACHE_VERSION = 'v1';

// Global cache variables
var g_cached_sleep_score = 0;
var g_cached_readiness_score = 0;
var g_cache_date = '';

var CACHE_KEYS = {
  READINESS: CACHE_VERSION + '_readiness',
  SLEEP: CACHE_VERSION + '_sleep',
  TIMESTAMP: CACHE_VERSION + '_timestamp'
};

// Cache functions
function getCachedData(key) {
  try {
    var data = localStorage.getItem(key);
    return data ? JSON.parse(data) : null;
  } catch (e) {
    console.log('Cache read error:', e);
    return null;
  }
}

function setCachedData(key, data) {
  try {
    localStorage.setItem(key, JSON.stringify(data));
    localStorage.setItem(CACHE_KEYS.TIMESTAMP, Date.now());
  } catch (e) {
    console.log('Cache write error:', e);
  }
}

function updateCacheIfValid(key, newValue) {
  if (!newValue) return false;
  
  var currentValue = getCachedData(key);
  // Only update if new value is valid (non-zero)
  if (newValue && newValue !== '0' && newValue !== 0) {
    setCachedData(key, newValue);
    return true;
  }
  return false;
}

function getCachedValue(key) {
  var cached = getCachedData(key);
  // Return cached value if it exists and is valid
  return (cached && cached !== '0' && cached !== 0) ? cached : null;
}

// OAuth2 configuration - SECURE CLIENT-SIDE-ONLY FLOW
// No client secret stored in browser - uses Oura's official client-side-only flow
var OURA_CONFIG = {
  CLIENT_ID: 'TGDTXUBGWULVNKSC',
  BASE_URL: 'https://api.ouraring.com/v2/usercollection',
  PROXY_URL: 'https://peppy-pothos-093b81.netlify.app/.netlify/functions/oura-proxy'
};

// Storage keys
var STORAGE_KEYS = {
  ACCESS_TOKEN: 'oura_access_token',
  REFRESH_TOKEN: 'oura_refresh_token',
  TOKEN_EXPIRES: 'oura_token_expires',
  LAST_UPDATE: 'oura_last_update'
};

// Global variables
var g_oura_token = null;
var g_api_completion_count = 0;
var g_total_apis = 3;
var g_aggregated_data = {};

// Persistent score cache to solve 0-0-65 problem
var g_cached_sleep_score = 0;
var g_cached_readiness_score = 0;
var g_cached_activity_score = 0;
var g_cache_date = null;

// Helper function to get local date string (YYYY-MM-DD) instead of UTC
// Compatible with older JavaScript environments (no padStart)
function getLocalDateString() {
  var now = new Date();
  var year = now.getFullYear();
  var month = now.getMonth() + 1;
  var day = now.getDate();
  
  // Manual padding for compatibility (no padStart in Pebble JS)
  var monthStr = month < 10 ? '0' + month : '' + month;
  var dayStr = day < 10 ? '0' + day : '' + day;
  
  return year + '-' + monthStr + '-' + dayStr;
}

// Helper function to get the appropriate date for Oura data
// Try today first (for fresh data), fall back to yesterday if needed
function getOuraDataDate() {
  var now = new Date();
  var year = now.getFullYear();
  var month = now.getMonth() + 1;
  var day = now.getDate();
  
  // Manual padding for compatibility (no padStart in Pebble JS)
  var monthStr = month < 10 ? '0' + month : '' + month;
  var dayStr = day < 10 ? '0' + day : '' + day;
  
  return year + '-' + monthStr + '-' + dayStr;
}

// Helper function to get yesterday's date as fallback
function getYesterdayDate() {
  var now = new Date();
  now.setDate(now.getDate() - 1);
  var year = now.getFullYear();
  var month = now.getMonth() + 1;
  var day = now.getDate();
  
  // Manual padding for compatibility (no padStart in Pebble JS)
  var monthStr = month < 10 ? '0' + month : '' + month;
  var dayStr = day < 10 ? '0' + day : '' + day;
  
  return year + '-' + monthStr + '-' + dayStr;
}

// Load cached values from localStorage on startup
function loadCachedScores() {
  try {
    var today = getLocalDateString();
    // One-time cache schema migration to purge stale activity caches
    var schema = localStorage.getItem('oura_cache_schema');
    if (schema !== '2') {
      console.log('Migrating cache schema to v2: purging old oura_cached_scores');
      localStorage.removeItem('oura_cached_scores');
      localStorage.setItem('oura_cache_schema', '2');
    }
    var cachedData = localStorage.getItem('oura_cached_scores');
    
    if (cachedData) {
      var data = JSON.parse(cachedData);
      console.log('Raw cached data:', data);
      
      // Only use cached data if it's from today
      if (data.cache_date === today) {
        if (data.sleep_score) {
          g_cached_sleep_score = parseInt(data.sleep_score);
          console.log('Loaded cached sleep score:', g_cached_sleep_score);
        }

      // Check if we got show_loading configuration
      if (settings.show_loading !== undefined && settings.show_loading !== null) {
        try {
          var sl = (settings.show_loading === true || settings.show_loading === 1 || settings.show_loading === '1');
          localStorage.setItem('oura_show_loading', sl ? '1' : '0'); // Use '1'/'0' format to match config page
          console.log('💾 show_loading stored:', sl);
          // Send immediately to watchface
          Pebble.sendAppMessage({ 'show_loading': sl ? 1 : 0 }, function() {
            console.log('✅ show_loading sent to watchface');
          }, function(err) {
            console.error('❌ Error sending show_loading:', err);
          });
        } catch (err) {
          console.error('Error handling show_loading setting:', err);
        }
      }
        if (data.readiness_score) {
          g_cached_readiness_score = parseInt(data.readiness_score);
          console.log('Loaded cached readiness score:', g_cached_readiness_score);
        }
        // Only load activity if activity_date matches today
        if (data.activity_score && data.activity_date === today) {
          g_cached_activity_score = parseInt(data.activity_score);
          console.log('Loaded cached activity score for today:', g_cached_activity_score);
        } else if (data.activity_score) {
          console.log('Cached activity score date mismatch or missing activity_date, ignoring activity cache');
        }
        g_cache_date = data.cache_date;
      } else {
        console.log('Cached data is from a different date, ignoring and clearing in-memory cached scores');
        // Clear in-memory cached values to avoid reusing stale scores
        g_cached_sleep_score = 0;
        g_cached_readiness_score = 0;
        g_cached_activity_score = 0;
        g_cache_date = null;
      }
    } else {
      console.log('No cached scores found in localStorage');
    }
  } catch (e) {
    console.error('Error loading cached scores:', e);
  }
}

// Save cached scores to localStorage - only saves if values are valid (non-zero)
function saveCachedScores() {
  try {
    var today = getLocalDateString();
    // Start with existing cache if present to avoid clobbering unrelated fields
    var existingRaw = localStorage.getItem('oura_cached_scores');
    var data = {};
    if (existingRaw) {
      try {
        data = JSON.parse(existingRaw) || {};
      } catch (e) {
        console.warn('Failed to parse existing cache, starting fresh');
        data = {};
      }
    }

    // Always update sleep and readiness from current values
    data.sleep_score = g_cached_sleep_score;
    data.readiness_score = g_cached_readiness_score;

    // Only update activity fields when we have an intentional activity update
    if (g_cached_activity_score > 0 && g_cache_date) {
      data.activity_score = g_cached_activity_score;
      data.activity_date = g_cache_date;
      console.log('Saving activity with date:', g_cache_date, 'score:', g_cached_activity_score);
    } else {
      console.log('Preserving existing activity in cache:', data.activity_date, data.activity_score);
    }

    // Cache-wide date for sleep/readiness; keep most recent cache date
    data.cache_date = g_cache_date || today;
    
    console.log('Saving cached scores:', data);
    localStorage.setItem('oura_cached_scores', JSON.stringify(data));
    console.log('Successfully saved cached scores to localStorage');
    
    // Debug: Verify the data was saved correctly
    var verify = localStorage.getItem('oura_cached_scores');
    console.log('Verification read from localStorage:', verify);
  } catch (e) {
    console.error('Error saving cached scores:', e);
  }
}

// Initialize cache
loadCachedScores();

// =============================================================================
// OAUTH2 AUTHENTICATION
// =============================================================================

function getStoredToken() {
  console.log('🔍 DIAGNOSTIC: Checking all token storage locations...');
  
  // Check all possible token locations
  var manualToken = localStorage.getItem('oura_access_token');
  var clayToken = localStorage.getItem('clay-oura_access_token');
  var webviewToken = localStorage.getItem(STORAGE_KEYS.ACCESS_TOKEN);
  var webviewExpires = localStorage.getItem(STORAGE_KEYS.TOKEN_EXPIRES);
  
  console.log('🔍 Manual token:', manualToken ? 'EXISTS (' + manualToken.length + ' chars)' : 'NONE');
  console.log('🔍 Clay token:', clayToken ? 'EXISTS (' + clayToken.length + ' chars)' : 'NONE');
  console.log('🔍 Webview token:', webviewToken ? 'EXISTS (' + webviewToken.length + ' chars)' : 'NONE');
  console.log('🔍 Webview expires:', webviewExpires);
  
  // Priority order: Clay token (from config page) > Manual token > Webview token
  if (clayToken) {
    console.log('✅ Using Clay token (highest priority)');
    return clayToken;
  }
  
  if (manualToken) {
    console.log('✅ Using manual setup token');
    return manualToken;
  }
  
  if (webviewToken && webviewExpires && Date.now() < parseInt(webviewExpires)) {
    console.log('✅ Using webview token');
    return webviewToken;
  }
  
  console.log('❌ No valid token found in any storage location');
  return null;
}

function storeTokens(accessToken, refreshToken, expiresIn) {
  var expiresAt = Date.now() + (expiresIn * 1000);
  
  localStorage.setItem(STORAGE_KEYS.ACCESS_TOKEN, accessToken);
  localStorage.setItem(STORAGE_KEYS.REFRESH_TOKEN, refreshToken);
  localStorage.setItem(STORAGE_KEYS.TOKEN_EXPIRES, expiresAt.toString());
  
  console.log('Oura tokens stored successfully');
}

// SECURE CLIENT-SIDE-ONLY FLOW - No token exchange needed
// Tokens come directly from OAuth redirect via configuration page
// This eliminates the need for client secrets in the browser

function checkForConfigurationSettings() {
  console.log('Checking for OAuth2 configuration settings...');
  
  // The secure-config.html page will store tokens directly in localStorage
  // after receiving them from Oura's OAuth redirect
  var token = getStoredToken();
  
  if (token) {
    console.log('Valid token found, fetching Oura data');
    sendDebugStatus('Token found, loading data...');
    fetchAllOuraData();
  } else {
    console.log('No valid token found, user needs to configure');
    sendDebugStatus('Please configure in Pebble app');
    // Do not send sample data; leave watchface blank until configured
  }
}

// CLIENT-SIDE-ONLY FLOW: No refresh tokens supported
// When tokens expire (30 days), user must re-authenticate via config page
function handleExpiredToken() {
  console.log('Token expired, user needs to re-authenticate via config page');
  sendDebugStatus('Token expired - please reconfigure');
  
  // Clear expired token
  localStorage.removeItem('oura_access_token');
  localStorage.removeItem('oura_token_expires');
  
  // Do not send sample data; wait for reconfiguration
}

// =============================================================================
// OURA API CALLS (Using XMLHttpRequest for ES5 compatibility)
// =============================================================================

function makeOuraRequest(endpoint, token, callback) {
  // Use proxy to work around Pebble JS HTTPS limitations
  console.log('[oura] 🔄 Making proxy request for endpoint:', endpoint);
  sendDebugStatus('Using proxy for API...');
  
  // Parse endpoint to extract the API endpoint name and parameters
  var endpointMatch = endpoint.match(/\/usercollection\/([^?]+)/);
  if (!endpointMatch) {
    console.error('❌ Invalid endpoint format:', endpoint);
    callback(new Error('Invalid endpoint format'), null);
    return;
  }
  
  var apiEndpoint = endpointMatch[1];
  
  // Extract date parameters from the endpoint (ES5-compatible)
  var query = endpoint.indexOf('?') !== -1 ? endpoint.split('?')[1] : '';
  var params = {};
  if (query) {
    var parts = query.split('&');
    for (var i = 0; i < parts.length; i++) {
      var kv = parts[i].split('=');
      var k = decodeURIComponent(kv[0] || '');
      var v = decodeURIComponent(kv.length > 1 ? kv[1] : '');
      if (k) { params[k] = v; }
    }
  }
  var startDate = params.start_date || null;
  var endDate = params.end_date || null;
  
  // Build proxy URL
  var proxyUrl = OURA_CONFIG.PROXY_URL + 
    '?endpoint=' + encodeURIComponent(apiEndpoint) +
    '&token=' + encodeURIComponent(token);
  
  if (startDate) proxyUrl += '&start_date=' + encodeURIComponent(startDate);
  if (endDate) proxyUrl += '&end_date=' + encodeURIComponent(endDate);
  
  console.log('[oura] 📡 Proxy URL:', proxyUrl.replace(token, token.substring(0, 10) + '...'));
  
  var xhr = new XMLHttpRequest();
  xhr.open('GET', proxyUrl, true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  var startedAt = Date.now();
  
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      var duration = Date.now() - startedAt;
      var respLen = (xhr.responseText && xhr.responseText.length) || 0;
      console.log('[oura] 📊 Proxy response status:', xhr.status, 'endpoint:', apiEndpoint, 'timeMs:', duration, 'len:', respLen);
      sendDebugStatus('Proxy status: ' + xhr.status);
      
      if (xhr.status === 200) {
        try {
          var data = JSON.parse(xhr.responseText);
          console.log('[oura] ✅ Proxy JSON parsed. Keys:', (data && Object.keys(data)) || []);
          sendDebugStatus('Data received via proxy!');
          callback(null, data);
        } catch (error) {
          console.error('[oura] ❌ JSON parse error:', error);
          console.log('[oura] ↪︎ Raw response (first 300 chars):', (xhr.responseText || '').substring(0, 300));
          sendDebugStatus('JSON parse error');
          callback(error, null);
        }
      } else {
        console.error('[oura] ❌ Proxy error status:', xhr.status);
        console.log('[oura] ↪︎ Error body (first 300 chars):', (xhr.responseText || '').substring(0, 300));
        sendDebugStatus('Proxy error: ' + xhr.status);
        callback(new Error('Proxy error: ' + xhr.status), null);
      }
    }
  };
  
  xhr.onerror = function() {
    console.error('[oura] ❌ Proxy network error');
    sendDebugStatus('Proxy network error');
    callback(new Error('Proxy network error'), null);
  };
  
  xhr.send();
}

function fetchHeartRateData(token, callback) {
  var dataDate = getOuraDataDate(); // Use yesterday's date for Oura data
  var endpoint = '/usercollection/heartrate?start_date=' + dataDate + '&end_date=' + dataDate;
  
  console.log('[oura] Fetching heart rate data for:', dataDate);
  sendDebugStatus('Getting heart rate...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch heart rate data:', error);
      sendDebugStatus('HR API failed');
      callback({ data_available: false });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      // Use the latest heart rate sample of the day (most recent timestamp)
      var latestIdx = -1;
      var latestTs = 0;
      var latestRmssd = 0;
      for (var i = 0; i < data.data.length; i++) {
        var rec = data.data[i];
        if (!rec || typeof rec.bpm !== 'number') { continue; }
        var t = 0;
        if (rec.timestamp) {
          var parsed = Date.parse(rec.timestamp);
          if (!isNaN(parsed)) { t = parsed; }
        }
        // If timestamp missing/unparseable, prefer later items by index
        if (t === 0) { t = i; }
        if (t >= latestTs) {
          latestTs = t;
          latestIdx = i;
          latestRmssd = (typeof rec.rmssd === 'number') ? rec.rmssd : latestRmssd;
        }
      }
      var latestBpm = (latestIdx >= 0) ? Math.round(data.data[latestIdx].bpm) : 0;
      console.log('[oura] HR records:', data.data.length, 'latest bpm:', latestBpm, 'latest rmssd:', latestRmssd);
      sendDebugStatus('HR latest: ' + latestBpm + ' bpm');
      callback({
        resting_heart_rate: latestBpm,
        hrv_score: latestRmssd || 0,
        data_available: true
      });
    } else {
      console.log('[oura] No heart rate data available');
      sendDebugStatus('No HR data today');
      callback({ data_available: false });
    }
  });
}

function fetchReadinessData(token, callback) {
  // Try today's data first
  var todayDate = getOuraDataDate();
  var endpoint = '/usercollection/daily_readiness?start_date=' + todayDate + '&end_date=' + todayDate;
  
  console.log('[oura] Fetching readiness data for today:', todayDate);
  sendDebugStatus('Getting readiness...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch readiness data:', error);
      sendDebugStatus('RDY API failed');
      
      // If API fails but we have a cached value, use it
      if (g_cached_readiness_score > 0) {
        console.log('[oura] Using cached readiness score after API error:', g_cached_readiness_score);
        sendDebugStatus('Using cached RDY');
        callback({
          readiness_score: g_cached_readiness_score,
          temperature_deviation: 0,
          recovery_index: 0,
          data_available: true
        });
        return;
      }
      
      callback({ data_available: false });
      return;
    }
    
    var result = { data_available: false };
    
    // Check if we have data for today
    if (data && data.data && data.data.length > 0) {
      var latestData = data.data[data.data.length - 1];
      var currentScore = latestData.score || 0;
      
      if (currentScore > 0) {
        // Fresh data available for today!
        g_cached_readiness_score = currentScore;
        g_cache_date = todayDate;
        console.log('[oura] Readiness: Fresh data for today:', currentScore);
        sendDebugStatus('RDY updated (today)');
        saveCachedScores();
        
        result = {
          readiness_score: currentScore,
          temperature_deviation: latestData.temperature_deviation || 0,
          recovery_index: latestData.recovery_index || 0,
          data_available: true
        };
        callback(result);
        return;
      }
    }
    
    // No data for today, try yesterday as fallback
    console.log('[oura] No readiness data for today, trying yesterday...');
    var yesterdayDate = getYesterdayDate();
    var fallbackEndpoint = '/usercollection/daily_readiness?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
    
    makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
      if (fallbackError) {
        console.error('Failed to fetch yesterday readiness data:', fallbackError);
        
        // Use cached value if available
        if (g_cached_readiness_score > 0) {
          console.log('[oura] Using cached readiness score');
          sendDebugStatus('Using cached RDY');
          callback({
            readiness_score: g_cached_readiness_score,
            temperature_deviation: 0,
            recovery_index: 0,
            data_available: true
          });
          return;
        }
        
        callback({ data_available: false });
        return;
      }
      
      if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
        var yesterdayData = fallbackData.data[fallbackData.data.length - 1];
        var yesterdayScore = yesterdayData.score || 0;
        
        if (yesterdayScore > 0) {
          // Only update cache if this is newer than what we have
          if (g_cache_date !== yesterdayDate) {
            g_cached_readiness_score = yesterdayScore;
            g_cache_date = yesterdayDate;
            console.log('[oura] Readiness: Updated with yesterday data:', yesterdayScore);
            sendDebugStatus('RDY updated (yesterday)');
            saveCachedScores();
          }
          
          callback({
            readiness_score: yesterdayScore,
            temperature_deviation: yesterdayData.temperature_deviation || 0,
            recovery_index: yesterdayData.recovery_index || 0,
            data_available: true
          });
          return;
        }
      }
      
      // Still no data, use cached if available
      if (g_cached_readiness_score > 0) {
        console.log('[oura] Using cached readiness score as final fallback');
        sendDebugStatus('Using cached RDY');
        callback({
          readiness_score: g_cached_readiness_score,
          temperature_deviation: 0,
          recovery_index: 0,
          data_available: true
        });
      } else {
        callback({ data_available: false });
      }
    });
  });
}

function fetchSleepData(token, callback) {
  // Try today's data first
  var todayDate = getOuraDataDate();
  var endpoint = '/usercollection/daily_sleep?start_date=' + todayDate + '&end_date=' + todayDate;
  
  console.log('[oura] Fetching sleep data for today:', todayDate);
  sendDebugStatus('Getting sleep...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch sleep data:', error);
      sendDebugStatus('Sleep API failed');
      
      // If API fails but we have a cached value, use it
      if (g_cached_sleep_score > 0) {
        console.log('[oura] Using cached sleep score after API error:', g_cached_sleep_score);
        sendDebugStatus('Using cached sleep');
        callback({
          sleep_score: g_cached_sleep_score,
          total_sleep_duration: 0,
          sleep_efficiency: 0,
          data_available: true
        });
        return;
      }
      
      callback({ data_available: false });
      return;
    }
    
    var result = { data_available: false };
    
    // Check if we have data for today
    if (data && data.data && data.data.length > 0) {
      var latestData = data.data[data.data.length - 1];
      var currentScore = latestData.score || 0;
      
      if (currentScore > 0) {
        // Fresh data available for today!
        g_cached_sleep_score = currentScore;
        g_cache_date = todayDate;
        console.log('[oura] Sleep: Fresh data for today:', currentScore);
        sendDebugStatus('Sleep updated (today)');
        saveCachedScores();
        
        result = {
          sleep_score: currentScore,
          total_sleep_duration: latestData.total_sleep_duration || 0,
          sleep_efficiency: latestData.efficiency || 0,
          data_available: true
        };
        callback(result);
        return;
      }
    }
    
    // No data for today, try yesterday as fallback
    console.log('[oura] No sleep data for today, trying yesterday...');
    var yesterdayDate = getYesterdayDate();
    var fallbackEndpoint = '/usercollection/daily_sleep?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
    
    makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
      if (fallbackError) {
        console.error('Failed to fetch yesterday sleep data:', fallbackError);
        
        // Use cached value if available
        if (g_cached_sleep_score > 0) {
          console.log('[oura] Using cached sleep score');
          sendDebugStatus('Using cached sleep');
          callback({
            sleep_score: g_cached_sleep_score,
            total_sleep_duration: 0,
            sleep_efficiency: 0,
            data_available: true
          });
          return;
        }
        
        callback({ data_available: false });
        return;
      }
      
      if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
        var yesterdayData = fallbackData.data[fallbackData.data.length - 1];
        var yesterdayScore = yesterdayData.score || 0;
        
        if (yesterdayScore > 0) {
          // Only update cache if this is newer than what we have
          if (g_cache_date !== yesterdayDate) {
            g_cached_sleep_score = yesterdayScore;
            g_cache_date = yesterdayDate;
            console.log('[oura] Sleep: Updated with yesterday data:', yesterdayScore);
            sendDebugStatus('Sleep updated (yesterday)');
            saveCachedScores();
          }
          
          callback({
            sleep_score: yesterdayScore,
            total_sleep_duration: yesterdayData.total_sleep_duration || 0,
            sleep_efficiency: yesterdayData.efficiency || 0,
            data_available: true
          });
          return;
        }
      }
      
      // Still no data, use cached if available
      if (g_cached_sleep_score > 0) {
        console.log('[oura] Using cached sleep score as final fallback');
        sendDebugStatus('Using cached sleep');
        callback({
          sleep_score: g_cached_sleep_score,
          total_sleep_duration: 0,
          sleep_efficiency: 0,
          data_available: true
        });
      } else {
        callback({ data_available: false });
      }
    });
  });
}

function fetchActivityData(token, callback) {
  // Try a wider date range to find activity data
  var todayDate = getOuraDataDate();
  var threeDaysAgo = new Date();
  threeDaysAgo.setDate(threeDaysAgo.getDate() - 3);
  var startDate = threeDaysAgo.toISOString().split('T')[0];
  
  var endpoint = '/usercollection/daily_activity?start_date=' + startDate + '&end_date=' + todayDate;
  
  console.log('[oura] Fetching activity data from', startDate, 'to', todayDate);
  sendDebugStatus('Getting activity (wide range)...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    console.log('[oura] ===== ACTIVITY DEBUG START =====');
    console.log('[oura] Activity: Raw response data:', data);
    if (data && data.data) {
      console.log('[oura] Activity: Data array length:', data.data.length);
      if (data.data.length > 0) {
        console.log('[oura] Activity: First item keys:', Object.keys(data.data[0]));
        console.log('[oura] Activity: First item score:', data.data[0].score);
      }
    }
    console.log('[oura] ===== ACTIVITY DEBUG END =====');
    
    if (error) {
      console.error('Failed to fetch activity data:', error);
      sendDebugStatus('Activity API failed');
      
      // Try cached activity score only if it's for TODAY
      try {
        var cachedRaw = localStorage.getItem('oura_cached_scores');
        var today = getLocalDateString();
        if (cachedRaw) {
          var cached = JSON.parse(cachedRaw);
          if (cached.activity_score > 0 && cached.activity_date === today) {
            console.log('[oura] Using cached TODAY activity after API error:', cached.activity_score);
            callback({
              activity_score: cached.activity_score,
              active_calories: 0,
              steps: 0,
              data_available: true
            });
            return;
          }
        }
      } catch (e) { console.warn('Activity cache parse error:', e); }
      
      // Try yesterday as fallback
      var yesterdayDate = getYesterdayDate();
      var fallbackEndpoint = '/usercollection/daily_activity?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
      console.log('[oura] Activity: Fallback request for yesterday:', fallbackEndpoint);
      
      makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
        if (fallbackError) {
          console.error('Failed to fetch yesterday activity data:', fallbackError);
          callback({ data_available: false });
          return;
        }
        
        console.log('[oura] Activity: Fallback response records:', fallbackData.data.length);
        if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
          var yesterdayActivity = fallbackData.data[fallbackData.data.length - 1];
          console.log('[oura] Activity: Fallback picked record day:', yesterdayActivity.day, 'score:', yesterdayActivity.score);
          // Only return data if we have a valid score
          if (yesterdayActivity.score > 0) {
            // Cache with yesterday date to prevent confusion
            g_cached_activity_score = yesterdayActivity.score;
            g_cache_date = yesterdayDate;
            saveCachedScores();
            sendDebugStatus('Activity yesterday ' + yesterdayDate + ': ' + yesterdayActivity.score);
            callback({
              activity_score: yesterdayActivity.score,
              active_calories: yesterdayActivity.active_calories || 0,
              steps: yesterdayActivity.steps || 0,
              data_available: true
            });
          } else {
            console.log('[oura] Activity: No valid yesterday score, marking as unavailable');
            callback({ data_available: false });
          }
        } else {
          callback({ data_available: false });
        }
      });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      console.log('[oura] Activity: TodayDate', todayDate, 'Found', data.data.length, 'activity records');
      for (var i = 0; i < data.data.length; i++) {
        var record = data.data[i];
        console.log('[oura] Activity record', i + ':', 'day=' + record.day, 'score=' + record.score);
      }

      // Prefer today's record explicitly by date match
      var todayRecord = null;
      for (var ti = 0; ti < data.data.length; ti++) {
        if (data.data[ti].day === todayDate) {
          todayRecord = data.data[ti];
          break;
        }
      }

      if (todayRecord) {
        // If there's a record for today, use it directly. If score is 0, treat as 0 (no fallback to yesterday).
        var todayScore = todayRecord.score || 0;
        console.log('[oura] Activity: Using TODAY record', todayDate, 'score:', todayScore);
        sendDebugStatus('Activity today ' + todayDate + ': ' + todayScore);
        g_cached_activity_score = todayScore;
        g_cache_date = todayDate;
        saveCachedScores();
        callback({
          activity_score: todayScore,
          active_calories: todayRecord.active_calories || 0,
          steps: todayRecord.steps || 0,
          data_available: true
        });
      } else {
        // If today's record missing entirely, check yesterday within same dataset
        var yesterdayDate = getYesterdayDate();
        var yRecord = null;
        for (var yi = 0; yi < data.data.length; yi++) {
          if (data.data[yi].day === yesterdayDate) {
            yRecord = data.data[yi];
            break;
          }
        }
        if (yRecord && (yRecord.score || 0) > 0) {
          var yScore = yRecord.score || 0;
          console.log('[oura] Activity: Using YESTERDAY record', yesterdayDate, 'score:', yScore);
          sendDebugStatus('Activity yesterday ' + yesterdayDate + ': ' + yScore);
          // Optionally cache yesterday with its date
          g_cached_activity_score = yScore;
          g_cache_date = yesterdayDate;
          saveCachedScores();
          callback({
            activity_score: yScore,
            active_calories: yRecord.active_calories || 0,
            steps: yRecord.steps || 0,
            data_available: true
          });
        } else {
          console.log('[oura] Activity: No valid today/yesterday score in range, marking as unavailable');
          callback({ data_available: false });
        }
      }
    } else {
      // No data for today, try yesterday
      console.log('[oura] No activity data for today, trying yesterday...');
      var yesterdayDate = getYesterdayDate();
      var fallbackEndpoint = '/usercollection/daily_activity?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
      
      makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
        if (fallbackError) {
          console.error('Failed to fetch yesterday activity data:', fallbackError);
          callback({ data_available: false });
          return;
        }
        
        if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
          var yesterdayActivity = fallbackData.data[fallbackData.data.length - 1];
          console.log('[oura] Activity: Updated with yesterday data, score:', yesterdayActivity.score);
          sendDebugStatus('Activity updated (yesterday)');
          
          // Only return data if we have a valid score
          if (yesterdayActivity.score > 0) {
            callback({
              activity_score: yesterdayActivity.score,
              active_calories: yesterdayActivity.active_calories || 0,
              steps: yesterdayActivity.steps || 0,
              data_available: true
            });
          } else {
            console.log('[oura] Activity: No valid yesterday score, marking as unavailable');
            callback({ data_available: false });
          }
        } else {
          // No data for yesterday either, try cached activity for TODAY only
          try {
            var cachedRaw2 = localStorage.getItem('oura_cached_scores');
            var today2 = getLocalDateString();
            if (cachedRaw2) {
              var cached2 = JSON.parse(cachedRaw2);
              if (cached2.activity_score > 0 && cached2.activity_date === today2) {
                console.log('[oura] Using cached TODAY activity after no data found:', cached2.activity_score);
                callback({
                  activity_score: cached2.activity_score,
                  active_calories: 0,
                  steps: 0,
                  data_available: true
                });
                return;
              }
            }
          } catch (e2) { console.warn('Activity cache parse error:', e2); }
          callback({ data_available: false });
        }
      });
    }
  });
}

function fetchStressData(token, callback) {
  // Try today's data first
  var todayDate = getOuraDataDate();
  var endpoint = '/usercollection/daily_stress?start_date=' + todayDate + '&end_date=' + todayDate;
  
  console.log('[oura] Fetching stress data for today:', todayDate);
  sendDebugStatus('Getting stress...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch stress data:', error);
      sendDebugStatus('Stress API failed');
      
      // Try yesterday as fallback
      var yesterdayDate = getYesterdayDate();
      var fallbackEndpoint = '/usercollection/daily_stress?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
      
      makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
        if (fallbackError) {
          console.error('Failed to fetch yesterday stress data:', fallbackError);
          callback({ data_available: false });
          return;
        }
        
        if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
          var yesterdayStress = fallbackData.data[fallbackData.data.length - 1];
          // Only return data if we have valid stress data
          if (yesterdayStress.stress_high !== undefined && yesterdayStress.stress_high !== null) {
            callback({
              stress_duration: yesterdayStress.stress_high,
              stress_high_duration: yesterdayStress.stress_high,
              data_available: true
            });
          } else {
            console.log('[oura] Stress: No valid yesterday data, marking as unavailable');
            callback({ data_available: false });
          }
        } else {
          callback({ data_available: false });
        }
      });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      var latestStress = data.data[data.data.length - 1];
      console.log('[oura] ===== STRESS DEBUG START =====');
      console.log('[oura] Stress: Raw stress data received:', JSON.stringify(latestStress));
      console.log('[oura] Stress: Available fields:', Object.keys(latestStress));
      console.log('[oura] Stress: stress_high value:', latestStress.stress_high);
      console.log('[oura] Stress: stress_high type:', typeof latestStress.stress_high);
      
      // Only return data if we have valid stress data
      if (latestStress.stress_high !== undefined && latestStress.stress_high !== null) {
        var stressSeconds = latestStress.stress_high;
        var stressMinutes = stressSeconds / 60; // Calculate minutes for display
        
        console.log('[oura] Stress: Final stress seconds (raw from API):', stressSeconds);
        console.log('[oura] Stress: Final stress minutes (calculated):', stressMinutes);
        console.log('[oura] ===== STRESS DEBUG END =====');
        sendDebugStatus('Stress updated (today)');
        
        callback({
          stress_duration: stressSeconds, // Send as seconds to match C code expectation
          stress_high_duration: stressSeconds,
          data_available: true
        });
      } else {
        console.log('[oura] Stress: No valid stress data found, marking as unavailable');
        callback({ data_available: false });
      }
    } else {
      // No data for today, try yesterday
      console.log('[oura] No stress data for today, trying yesterday...');
      var yesterdayDate = getYesterdayDate();
      var fallbackEndpoint = '/usercollection/daily_stress?start_date=' + yesterdayDate + '&end_date=' + yesterdayDate;
      
      makeOuraRequest(fallbackEndpoint, token, function(fallbackError, fallbackData) {
        if (fallbackError) {
          console.error('Failed to fetch yesterday stress data:', fallbackError);
          callback({ data_available: false });
          return;
        }
        
        if (fallbackData && fallbackData.data && fallbackData.data.length > 0) {
          var yesterdayStress = fallbackData.data[fallbackData.data.length - 1];
          console.log('[oura] Stress: Raw yesterday stress data:', JSON.stringify(yesterdayStress));
          console.log('[oura] Stress: Available yesterday fields:', Object.keys(yesterdayStress));
          
          // Only return data if we have valid stress data
          if (yesterdayStress.stress_high !== undefined && yesterdayStress.stress_high !== null) {
            var stressSeconds = yesterdayStress.stress_high;
            var stressMinutes = stressSeconds / 60; // Calculate minutes for display
            
            console.log('[oura] Stress: Yesterday stress_high field:', yesterdayStress.stress_high, 'seconds');
            console.log('[oura] Stress: Yesterday stress minutes calculated:', stressMinutes, 'minutes');
            sendDebugStatus('Stress updated (yesterday)');
            
            callback({
              stress_duration: stressSeconds, // Send as seconds to match C code expectation
              stress_high_duration: stressSeconds,
              data_available: true
            });
          } else {
            console.log('[oura] Stress: No valid yesterday stress data, marking as unavailable');
            callback({ data_available: false });
          }
        } else {
          callback({ data_available: false });
        }
      });
    }
  });
}

// =============================================================================
// DATA AGGREGATION AND WATCH COMMUNICATION
// =============================================================================

function fetchAllOuraData() {
  console.log('🚀 Starting to fetch all Oura data...');
  
  var token = CONFIG_SETTINGS.access_token;
  if (!token || !CONFIG_SETTINGS.connected) {
    console.log('❌ No token available in CONFIG_SETTINGS');
    sendDebugStatus('No token - please configure');
    return;
  }
  
  console.log('🔐 Token available:', token.substring(0, 10) + '...' + token.substring(token.length - 6));
  console.log('📊 Token length:', token.length);
  sendDebugStatus('Token found - fetching data');
  
  // Check token expiration using CONFIG_SETTINGS
  if (CONFIG_SETTINGS.token_expires) {
    var isExpired = Date.now() > CONFIG_SETTINGS.token_expires;
    console.log('⏰ Token expires:', new Date(CONFIG_SETTINGS.token_expires).toISOString());
    console.log('🔍 Token expired:', isExpired);
    
    if (isExpired) {
      console.log('❌ Token is expired');
      sendDebugStatus('Token expired - need reauth');
      return;
    }
  }
  
  console.log('✅ Token valid, fetching real data');
  sendDebugStatus('Fetching from Oura API...');
  // Use the aggregator that waits for all 3 API calls, then sends to the watch
  console.log('📡 Starting aggregated API calls (3 total)');
  fetchAllOuraDataLegacy(token);
}

function fetchAllOuraDataLegacy(token) {
  var results = {
    heart_rate: null,
    readiness: null,
    sleep: null,
    activity: null,
    stress: null
  };
  
  var completed = 0;
  var total = 5;
  
  function checkComplete() {
    completed++;
    console.log('API call completed:', completed, 'of', total);
    sendDebugStatus('API ' + completed + '/' + total + ' done');
    
    if (completed > total) {
      console.error('ERROR: More completions than expected!', completed, '>', total);
      sendDebugStatus('ERROR: Too many calls!');
      return; // Prevent multiple data sends
    }
    
    if (completed === total) {
      var ouraData = {
        heart_rate: results.heart_rate || { data_available: false },
        readiness: results.readiness || { data_available: false },
        sleep: results.sleep || { data_available: false },
        activity: results.activity || { data_available: false },
        stress: results.stress || { data_available: false },
        last_updated: Date.now()
      };
      
      console.log('All Oura data fetched:', ouraData);
      sendDebugStatus('Sending real data!');
      localStorage.setItem(STORAGE_KEYS.LAST_UPDATE, Date.now().toString());
      setTimeout(function() {
        sendDataToWatch(ouraData);
      }, ACTIVITY_SEND_DELAY_MS);
    }
  }
  
  fetchHeartRateData(token, function(data) {
    console.log('Heart rate callback received:', data);
    results.heart_rate = data;
    sendDebugStatus('HR callback done');
    checkComplete();
  });
  
  fetchReadinessData(token, function(data) {
    console.log('Readiness callback received:', data);
    results.readiness = data;
    sendDebugStatus('RDY callback done');
    checkComplete();
  });
  
  fetchSleepData(token, function(data) {
    console.log('Sleep callback received:', data);
    results.sleep = data;
    sendDebugStatus('Sleep callback done');
    checkComplete();
  });
  
  fetchActivityData(token, function(data) {
    console.log('Activity callback received:', data);
    results.activity = data;
    sendDebugStatus('Activity callback done');
    checkComplete();
  });
  
  fetchStressData(token, function(data) {
    console.log('Stress callback received:', data);
    results.stress = data;
    sendDebugStatus('Stress callback done');
    checkComplete();
  });
}

function loadSampleData() {
  console.log('Loading sample data');
  sendDebugStatus('Using sample data');
  sendSampleDataToWatch();
}

// Get currently cached Oura data for immediate layout updates
function getCachedOuraData() {
  // Return cached data if available
  if (g_cached_readiness_score > 0 || g_cached_sleep_score > 0) {
    return {
      readiness: {
        readiness_score: g_cached_readiness_score,
        temperature_deviation: 0,
        recovery_index: g_cached_readiness_score,
        data_available: g_cached_readiness_score > 0
      },
      sleep: {
        sleep_score: g_cached_sleep_score,
        total_sleep_time: 450, // Default reasonable value
        deep_sleep_time: 90,   // Default reasonable value
        data_available: g_cached_sleep_score > 0
      },
      heart_rate: {
        resting_heart_rate: 65, // Default reasonable value
        hrv_score: 45,          // Default reasonable value
        data_available: true
      }
    };
  }
  return null; // No cached data available
}

function sendSampleDataToWatch() {
  var sampleData = {
    heart_rate: {
      resting_heart_rate: 65,
      hrv_score: 45,
      data_available: true
    },
    readiness: {
      readiness_score: 85,
      temperature_deviation: 0,
      recovery_index: 82,
      data_available: true
    },
    sleep: {
      sleep_score: 78,
      total_sleep_time: 450,
      deep_sleep_time: 90,
      data_available: true
    },
    activity: {
      activity_score: 85,
      active_calories: 320,
      steps: 8500,
      data_available: true
    },
    stress: {
      stress_duration: 720, // 12 minutes in seconds
      stress_high_duration: 720,
      data_available: true
    }
  };
  
  console.log('Sending sample Oura data to watch');
  sendDataToWatch(sampleData);
}

var DEBUG_THROTTLE_MS = 1000; // min gap between debug sends
var g_last_debug_sent_at = 0;
var g_pending_debug = null;
function sendDebugStatus(message) {
  // Respect user setting: when disabled, suppress debug messages entirely
  if (typeof CONFIG_SETTINGS !== 'undefined' && CONFIG_SETTINGS && CONFIG_SETTINGS.show_debug === false) {
    return;
  }
  function doSend(msg) {
    enqueueMessage({ 'debug_status': msg }, function(){
      console.log('Debug status sent:', msg);
    }, function(err){
      console.error('Failed to send debug status:', err);
    });
    g_last_debug_sent_at = Date.now();
  }
  var now = Date.now();
  if (now - g_last_debug_sent_at >= DEBUG_THROTTLE_MS) {
    doSend(message);
  } else {
    // coalesce into a single pending debug update
    g_pending_debug = message;
    setTimeout(function(){
      if (g_pending_debug) {
        var m = g_pending_debug; g_pending_debug = null;
        doSend(m);
      }
    }, DEBUG_THROTTLE_MS);
  }
  // Auto-clear debug message after 5 minutes
  setTimeout(function() {
    enqueueMessage({ 'debug_status': '' }, function(){
      console.log('Debug status cleared after 5 minutes');
    }, function(err){
      console.error('Failed to clear debug status:', err);
    });
  }, 5 * 60 * 1000);
}

function sendDataToWatch(data) {
  // Convert nested data structure to flat message keys that C code expects
  var flatData = {};
  
  // Add measurement layout configuration
  var savedLayout = localStorage.getItem('oura_measurement_layout');
  if (savedLayout) {
    try {
      var layoutConfig = JSON.parse(savedLayout);
      // Config page saves numeric strings ('0', '1', '2'), convert to integers
      // 0=readiness, 1=sleep, 2=heart_rate
      
      // Use proper fallback that doesn't treat 0 as falsy
      flatData.layout_left = (layoutConfig.left !== undefined && layoutConfig.left !== null) ? parseInt(layoutConfig.left) : 0;
      flatData.layout_middle = (layoutConfig.middle !== undefined && layoutConfig.middle !== null) ? parseInt(layoutConfig.middle) : 1;
      flatData.layout_right = (layoutConfig.right !== undefined && layoutConfig.right !== null) ? parseInt(layoutConfig.right) : 2;
      
      // Add flexible layout fields for 2-row support
      flatData.layout_rows = (layoutConfig.rows !== undefined && layoutConfig.rows !== null) ? parseInt(layoutConfig.rows) : 1;
      flatData.row1_left = flatData.layout_left;
      flatData.row1_middle = flatData.layout_middle;
      flatData.row1_right = flatData.layout_right;
      flatData.row2_left = (layoutConfig.row2_left !== undefined && layoutConfig.row2_left !== null) ? parseInt(layoutConfig.row2_left) : 3;
      flatData.row2_right = (layoutConfig.row2_right !== undefined && layoutConfig.row2_right !== null) ? parseInt(layoutConfig.row2_right) : 4;
      
      console.log('[oura] Sending layout config:', layoutConfig, '-> positions:', 
                  flatData.layout_left, flatData.layout_middle, flatData.layout_right);
      console.log('[oura] Sending flexible layout: rows=' + flatData.layout_rows + 
                  ', row2_left=' + flatData.row2_left + ', row2_right=' + flatData.row2_right);
    } catch (e) {
      console.log('[oura] Error parsing layout config, using defaults:', e);
      // Default layout: readiness-sleep-heart_rate
      flatData.layout_left = 0;
      flatData.layout_middle = 1;
      flatData.layout_right = 2;
    }
  } else {
    // Default layout: readiness-sleep-heart_rate
    flatData.layout_left = 0;
    flatData.layout_middle = 1;
    flatData.layout_right = 2;
    console.log('[oura] No saved layout, using default positions');
  }
  
  // Add date format configuration - always get the most current value from localStorage
  var dateFormat = localStorage.getItem('oura_date_format') || '0'; // Use same key as config page
  flatData.date_format = parseInt(dateFormat); // 0 = MM-DD-YYYY, 1 = DD-MM-YYYY
  var formatName = (flatData.date_format === 1) ? 'DD-MM-YYYY' : 'MM-DD-YYYY';
  console.log('[oura] Sending date format:', formatName, '-> value:', flatData.date_format);
  
  // Add theme mode configuration - re-enabled
  var themeMode = localStorage.getItem('oura_theme_mode'); // '0'=Dark, '1'=Light, '2'=Custom
  if (themeMode === null || themeMode === undefined || themeMode === '') {
    themeMode = '0';
  }
  flatData.theme_mode = parseInt(themeMode);
  var themeName = (flatData.theme_mode === 1) ? '☀️ Light Mode' : (flatData.theme_mode === 2 ? '🎨 Custom' : '🌙 Dark Mode');
  console.log('[oura] Sending theme mode:', themeName, '-> value:', flatData.theme_mode);

  // If using Custom Color, include the selected custom_color_index
  if (flatData.theme_mode === 2) {
    try {
      var savedColorJson = localStorage.getItem('oura_custom_color');
      if (savedColorJson) {
        var savedColor = JSON.parse(savedColorJson);
        if (savedColor && typeof savedColor.index === 'number') {
          flatData.custom_color_index = savedColor.index;
          console.log('[oura] Sending custom_color_index:', flatData.custom_color_index);
        }
      }
    } catch (e) {
      console.log('Error reading custom color selection:', e);
    }
  }

  // Include show_loading preference so watchface can control overlay per refresh
  try {
    var showLoadingPref = localStorage.getItem('oura_show_loading');
    // Config page stores '1'/'0', default to '0' (false) if not set
    var showLoading = (showLoadingPref === '1');
    flatData.show_loading = showLoading ? 1 : 0;
    console.log('[oura] Sending show_loading:', flatData.show_loading);
  } catch (e) {
    flatData.show_loading = 0; // Default to false (no loading screen)
  }
  
  // Heart rate data
  if (data.heart_rate) {
    flatData.heart_rate = 1; // Indicate heart rate data present
    flatData.resting_heart_rate = data.heart_rate.resting_heart_rate || 0;
    flatData.hrv_score = data.heart_rate.hrv_score || 0;
    flatData.data_available = data.heart_rate.data_available ? 1 : 0;
  }
  
  // Readiness data
  if (data.readiness) {
    flatData.readiness = 1; // Indicate readiness data present
    flatData.readiness_score = data.readiness.readiness_score || 0;
    flatData.temperature_deviation = data.readiness.temperature_deviation || 0;
    flatData.recovery_index = data.readiness.recovery_index || 0;
    // Note: reusing data_available key - this might cause issues, need separate keys
  }
  
  // Sleep data
  if (data.sleep) {
    flatData.sleep = 1; // Indicate sleep data present
    flatData.sleep_score = data.sleep.sleep_score || 0;
    flatData.total_sleep_time = data.sleep.total_sleep_time || 0;
    flatData.deep_sleep_time = data.sleep.deep_sleep_time || 0;
    // Note: reusing data_available key - this might cause issues, need separate keys
  }
  
  // Activity data
  if (data.activity) {
    if (data.activity.data_available) {
      flatData.activity_score = data.activity.activity_score || 0;
      flatData.active_calories = data.activity.active_calories || 0;
      flatData.steps = data.activity.steps || 0;
      console.log('[oura] Activity data included:', data.activity);
    } else {
      // Mark as unavailable by sending zeros so C shows "--"
      flatData.activity_score = 0;
      flatData.active_calories = 0;
      flatData.steps = 0;
      console.log('[oura] Activity unavailable - sending zeros');
    }
  } else {
    // No activity block present; ensure zeros
    flatData.activity_score = 0;
    flatData.active_calories = 0;
    flatData.steps = 0;
    console.log('[oura] No activity data to send');
  }
  
  // Stress data - only include when available; allow 0 seconds as valid measurement
  if (data.stress && data.stress.data_available) {
    flatData.stress_duration = data.stress.stress_duration || 0; // seconds
    flatData.stress_high_duration = data.stress.stress_high_duration || 0;
    console.log('[oura] Stress data included:', data.stress);
    console.log('[oura] Stress duration being sent:', flatData.stress_duration, 'seconds');
  } else {
    console.log('[oura] Stress unavailable - not sending stress fields');
  }
  
  // Signal that this is a complete aggregated payload
  flatData.payload_complete = 1;
  
  console.log('[oura] Sending flattened data to watch (with layout):', flatData);
  
  enqueueMessage(flatData,
    function() {
      console.log('[oura] Data sent to watch successfully');
    }, function(error) {
      console.error('[oura] Failed to send data to watch:', error);
    }
  );
}

// =============================================================================
// PEBBLE EVENT HANDLERS
// =============================================================================

// Configuration settings (loaded from config page)
var CONFIG_SETTINGS = {
  access_token: null,
  refresh_token: null,
  token_expires: null,
  refresh_frequency: 30, // minutes
  show_debug: true
};

// Load settings from Clay configuration or localStorage
function loadConfigSettings() {
  console.log('🔧 Loading configuration settings...');
  
  // Debug: Check all localStorage keys
  console.log('🔍 All localStorage keys:', Object.keys(localStorage));
  
  // Check for Clay configuration first (priority)
  var clayToken = localStorage.getItem('clay-oura_access_token');
  var clayRefresh = localStorage.getItem('clay-refresh_frequency');
  var clayDebug = localStorage.getItem('clay-show_debug');
  
  console.log('🏺 Clay token found:', !!clayToken);
  if (clayToken) {
    console.log('🏺 Clay token length:', clayToken.length);
  }
  
  if (clayToken) {
    CONFIG_SETTINGS.access_token = clayToken;
    CONFIG_SETTINGS.refresh_frequency = parseInt(clayRefresh) || 30;
    CONFIG_SETTINGS.show_debug = clayDebug === 'true';
    CONFIG_SETTINGS.token_expires = Date.now() + (30 * 24 * 60 * 60 * 1000); // 30 days
    CONFIG_SETTINGS.connected = true;
    
    console.log('✅ Clay configuration detected - using Clay settings');
  } else {
    // Check for manual setup tokens (fallback)
    var manualToken = localStorage.getItem('oura_access_token');
    var manualExpires = localStorage.getItem('oura_token_expires');
    var manualConnected = localStorage.getItem('oura_connected');
    
    console.log('🔧 Manual token found:', !!manualToken);
    if (manualToken) {
      console.log('🔧 Manual token length:', manualToken.length);
      console.log('🔧 Manual expires:', manualExpires);
      console.log('🔧 Manual connected:', manualConnected);
    }
    
    if (manualToken) {
      CONFIG_SETTINGS.access_token = manualToken;
      CONFIG_SETTINGS.token_expires = parseInt(manualExpires) || 0;
      CONFIG_SETTINGS.connected = manualConnected === 'true';
      // Default to showing debug unless explicitly disabled by user setting
      var storedShowDebug = localStorage.getItem('oura_show_debug');
      // If key missing, default true. If key present, honor 'true'/'false'
      CONFIG_SETTINGS.show_debug = (storedShowDebug === null || storedShowDebug === undefined) ? true : (storedShowDebug === 'true');
      CONFIG_SETTINGS.refresh_frequency = 30;
      
      console.log('✅ Manual setup detected - using manual token');
    } else {
      console.log('❌ No tokens found in localStorage');
    }
  }
  
  // Validate token expiration
  if (CONFIG_SETTINGS.access_token && CONFIG_SETTINGS.token_expires > Date.now()) {
    CONFIG_SETTINGS.connected = true;
  } else {
    CONFIG_SETTINGS.connected = false;
  }
  
  console.log('Final config state:', {
    hasToken: !!CONFIG_SETTINGS.access_token,
    connected: CONFIG_SETTINGS.connected,
    expiresAt: new Date(CONFIG_SETTINGS.token_expires).toISOString(),
    showDebug: CONFIG_SETTINGS.show_debug,
    showLoading: CONFIG_SETTINGS.show_loading,
    refreshFreq: CONFIG_SETTINGS.refresh_frequency
  });
}



Pebble.addEventListener('ready', function() {
  console.log('Oura Stats Watchface JS ready - Secure Client-Side-Only Flow');
  
  // Load configuration settings
  loadConfigSettings();
  // Load show_loading preference (defaults to false)
  try {
    var storedShowLoading = localStorage.getItem('oura_show_loading');
    if (typeof CONFIG_SETTINGS !== 'undefined') {
      // Config page stores '1'/'0', default to false if not set
      CONFIG_SETTINGS.show_loading = (storedShowLoading === '1');
    }
  } catch (e) {
    console.log('Error loading show_loading, defaulting false', e);
    if (typeof CONFIG_SETTINGS !== 'undefined') CONFIG_SETTINGS.show_loading = false;
  }
  
  if (CONFIG_SETTINGS.show_debug) {
    sendDebugStatus('JS Ready');
  }
  
  // Immediately send show_loading preference to the watchface
  try {
    var slPref = localStorage.getItem('oura_show_loading');
    // Config page stores '1'/'0', default to false if not set
    var slVal = (slPref === '1');
    CONFIG_SETTINGS.show_loading = slVal;
    Pebble.sendAppMessage({ 'show_loading': slVal ? 1 : 0 }, function() {
      console.log('✅ Initial show_loading sent:', slVal ? 1 : 0);
    }, function(err) {
      console.error('❌ Error sending initial show_loading:', err);
    });
  } catch (e) {
    console.log('Error determining show_loading on ready:', e);
  }

  // Check if we have a valid token and fetch data
  if (CONFIG_SETTINGS.connected && CONFIG_SETTINGS.access_token) {
    console.log('Valid token found in CONFIG_SETTINGS, fetching Oura data');
    sendDebugStatus('Token found, loading data...');
    fetchAllOuraData();
  } else {
    console.log('No valid token found in CONFIG_SETTINGS');
    sendDebugStatus('Please configure in Pebble app');
  }
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('Received message from watch:', e.payload);
  
  if (e.payload.request_data) {
    // Ensure show_loading is sent right before data fetch cycle
    try {
      var slPrefNow = localStorage.getItem('oura_show_loading');
      // Config page stores '1'/'0', default to false if not set
      var slNow = (slPrefNow === '1');
      Pebble.sendAppMessage({ 'show_loading': slNow ? 1 : 0 }, function() {
        console.log('✅ show_loading re-sent on request_data:', slNow ? 1 : 0);
      }, function(err) {
        console.error('❌ Error re-sending show_loading on request_data:', err);
      });
    } catch (e2) { console.log('Error re-sending show_loading:', e2); }
    fetchAllOuraData();
  }
  
  if (e.payload.setup_auth) {
    initiateOAuth();
  }
  
  // Handle OAuth2 authorization code from watch
  if (e.payload.auth_code) {
    console.log('Received authorization code from watch');
    exchangeCodeForToken(e.payload.auth_code);
  }
});

// Show configuration page when user taps Settings
Pebble.addEventListener('showConfiguration', function() {
  var configUrl = 'https://peppy-pothos-093b81.netlify.app/pebble-static-config.html';
  console.log('Opening configuration page:', configUrl);
  Pebble.openURL(configUrl);
});



// Handle configuration settings from config page
Pebble.addEventListener('webviewclosed', function(e) {
  console.log('🔧 Configuration closed:', e.response);
  sendDebugStatus('Config page closed');
  
  if (e.response) {
    try {
      console.log('🔍 Raw response before decode:', e.response);
      var settings = JSON.parse(decodeURIComponent(e.response));
      console.log('📥 Received config settings:', JSON.stringify(settings));
      console.log('🔍 Settings keys:', Object.keys(settings));
      sendDebugStatus('Settings received: ' + Object.keys(settings).join(', '));
      
      // Check if we got a token
      if (settings.oura_access_token) {
        console.log('🔐 New token received:', settings.oura_access_token.substring(0, 10) + '...');
        sendDebugStatus('New token received');
        
        // Store the token using our storage function
        localStorage.setItem('oura_access_token', settings.oura_access_token);
        localStorage.setItem('oura_token_expires', Date.now() + (30 * 24 * 60 * 60 * 1000)); // 30 days
        console.log('💾 Token stored in localStorage');
        sendDebugStatus('Token stored');
      }
      
      // Check if we got layout configuration
      console.log('🔍 Checking for layout config - left:', settings.layout_left, 'middle:', settings.layout_middle, 'right:', settings.layout_right);
      if (settings.layout_left !== undefined && settings.layout_left !== null && 
          settings.layout_middle !== undefined && settings.layout_middle !== null && 
          settings.layout_right !== undefined && settings.layout_right !== null) {
        console.log('📊 Layout configuration received:', settings.layout_left, settings.layout_middle, settings.layout_right);
        sendDebugStatus('Layout config received');
        
        // Store layout configuration in localStorage (config page already saved it, but ensure consistency)
        var layoutConfig = {
          left: settings.layout_left.toString(),
          middle: settings.layout_middle.toString(),
          right: settings.layout_right.toString(),
          // Add flexible layout fields from config page
          rows: (settings.layout_rows !== undefined && settings.layout_rows !== null) ? settings.layout_rows.toString() : '1',
          row2_left: (settings.row2_left !== undefined && settings.row2_left !== null) ? settings.row2_left.toString() : '3',
          row2_right: (settings.row2_right !== undefined && settings.row2_right !== null) ? settings.row2_right.toString() : '4'
        };
        console.log('💾 Storing layout config:', JSON.stringify(layoutConfig));
        console.log('🔍 Flexible layout fields - rows:', layoutConfig.rows, 'row2_left:', layoutConfig.row2_left, 'row2_right:', layoutConfig.row2_right);
        localStorage.setItem('oura_measurement_layout', JSON.stringify(layoutConfig));
        console.log('✅ Layout configuration stored in localStorage');
        sendDebugStatus('Layout config stored');
      }
      
      // Check if we got date format configuration
      if (settings.date_format !== undefined && settings.date_format !== null) {
        console.log('📅 Date format configuration received:', settings.date_format);
        sendDebugStatus('Date format config received');
        
        // Store date format configuration
        localStorage.setItem('oura_date_format', settings.date_format.toString());
        console.log('💾 Date format stored:', settings.date_format);
        sendDebugStatus('Date format stored');
        
        // Send date format to watchface immediately
        try {
          var message = {
            'date_format': parseInt(settings.date_format)
          };
          Pebble.sendAppMessage(message, function() {
            console.log('✅ Date format sent to watchface');
            sendDebugStatus('Date format applied');
          }, function(error) {
            console.error('❌ Error sending date format:', error);
            sendDebugStatus('Error applying date format');
          });
        } catch (error) {
          console.error('❌ Error sending date format message:', error);
        }
      }
      
      // Check if we got theme mode configuration
      if (settings.theme_mode !== undefined && settings.theme_mode !== null) {
        console.log('🎨 Theme mode configuration received:', settings.theme_mode);
        sendDebugStatus('Theme mode config received');
        
        // Store theme mode configuration
        localStorage.setItem('oura_theme_mode', settings.theme_mode.toString());
        console.log('💾 Theme mode stored:', settings.theme_mode);
        sendDebugStatus('Theme mode stored');
        
        // Send theme mode to watchface immediately
        try {
          var message = {
            'theme_mode': parseInt(settings.theme_mode)
          };
          Pebble.sendAppMessage(message, function() {
            console.log('✅ Theme mode sent to watchface');
            sendDebugStatus('Theme mode applied');
          }, function(error) {
            console.error('❌ Error sending theme mode:', error);
            sendDebugStatus('Error applying theme mode');
          });
        } catch (error) {
          console.error('❌ Error sending theme mode message:', error);
        }
      }

      // Handle custom color when present
      if (settings.custom_color_index !== undefined && settings.custom_color_index !== null) {
        try {
          // Persist full object when available
          var colorObj = {
            index: parseInt(settings.custom_color_index)
          };
          if (settings.custom_color_name) colorObj.name = settings.custom_color_name;
          if (settings.custom_color_hex) colorObj.hex = settings.custom_color_hex;
          if (settings.custom_color_pebble) colorObj.pebble = settings.custom_color_pebble;
          localStorage.setItem('oura_custom_color', JSON.stringify(colorObj));
          console.log('💾 Stored custom color selection:', JSON.stringify(colorObj));

          // Send to watch immediately
          Pebble.sendAppMessage({ 'custom_color_index': colorObj.index }, function() {
            console.log('✅ custom_color_index sent to watchface');
          }, function(err) {
            console.error('❌ Error sending custom_color_index:', err);
          });
        } catch (e2) {
          console.error('❌ Error handling custom color settings:', e2);
        }
      }
      
      // Process layout changes if received
      if (settings.layout_left !== undefined && settings.layout_left !== null && 
          settings.layout_middle !== undefined && settings.layout_middle !== null && 
          settings.layout_right !== undefined && settings.layout_right !== null) {
        
        // Immediately apply the new layout with current data
        console.log('🔄 Applying new layout immediately...');
        sendDebugStatus('Applying new layout');
        
        // Get current cached data and resend with new layout
        console.log('🔍 Getting cached data...');
        var cachedData = getCachedOuraData();
        console.log('📊 Cached data result:', cachedData ? 'Found' : 'None');
        
        if (cachedData && (cachedData.readiness || cachedData.sleep || cachedData.heart_rate)) {
          console.log('📊 Resending cached data with new layout');
          sendDebugStatus('Resending cached data');
          try {
            sendDataToWatch(cachedData);
            console.log('✅ Data sent to watch with new layout');
            sendDebugStatus('Layout applied successfully');
          } catch (error) {
            console.error('❌ Error sending data to watch:', error);
            sendDebugStatus('Error applying layout: ' + error.message);
          }
        } else {
          // If no cached data, fetch fresh data to apply new layout
          console.log('🔄 No cached data, fetching fresh data for new layout');
          sendDebugStatus('Fetching fresh data for layout');
          try {
            fetchAllOuraData();
          } catch (error) {
            console.error('❌ Error fetching fresh data:', error);
            sendDebugStatus('Error fetching data: ' + error.message);
          }
        }
      } else {
        console.log('⚠️ Layout configuration not found in settings');
        sendDebugStatus('No layout config in settings');
      }
      
      // Update configuration
      // ES5-safe merge (Object.assign is not available)
      for (var key in settings) {
        if (settings.hasOwnProperty(key)) {
          CONFIG_SETTINGS[key] = settings[key];
        }
      }
      
      // Store settings for persistence
      localStorage.setItem('oura_config_settings', JSON.stringify(CONFIG_SETTINGS));
      console.log('⚙️ Config settings updated and stored');
      
      // Update periodic refresh interval if changed
      updateRefreshInterval();
      
      // Check what token we have now
      var currentToken = getStoredToken();
      if (currentToken) {
        console.log('✅ Current token available:', currentToken.substring(0, 10) + '...');
        sendDebugStatus('Token available - fetching data');
        fetchAllOuraData();
      } else {
        console.log('❌ No token available after config');
        sendDebugStatus('No token available');
      }
      
    } catch (error) {
      console.error('❌ Error parsing config response:', error);
      sendDebugStatus('Config parse error: ' + error.message);
    }
  } else {
    console.log('⚠️ Configuration closed without response');
    sendDebugStatus('Config closed - no data');
  }
});

// =============================================================================
// PERIODIC DATA UPDATES
// =============================================================================

// Dynamic refresh interval (will be updated by config)
var refreshIntervalId = null;

function updateRefreshInterval() {
  // Clear existing interval
  if (refreshIntervalId) {
    clearInterval(refreshIntervalId);
  }
  
  // Set new interval based on config
  var refreshMinutes = CONFIG_SETTINGS.refresh_frequency || 30;
  var refreshMs = refreshMinutes * 60 * 1000;
  
  console.log('Setting refresh interval to', refreshMinutes, 'minutes');
  
  refreshIntervalId = setInterval(function() {
    var lastUpdate = localStorage.getItem(STORAGE_KEYS.LAST_UPDATE);
    var refreshAgo = Date.now() - refreshMs;
    
    if (!lastUpdate || parseInt(lastUpdate) < refreshAgo) {
      console.log('Periodic Oura data update (' + refreshMinutes + ' min interval)');
      fetchAllOuraData();
    }
  }, refreshMs);
}

// Initialize refresh interval
updateRefreshInterval();

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

function setPersonalAccessToken(token) {
  storeTokens(token, null, 365 * 24 * 60 * 60);
  console.log('Personal access token set');
  fetchAllOuraData();
}

console.log('Oura Stats Watchface JavaScript component loaded');
