// Clay configuration handler for Oura Stats
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');

// Custom function to handle the "Get Oura Token" button
var customClay = function(minified) {
  var clayConfig = this;
  var $ = minified;
  
  // Handle the "Get Oura Token" button click
  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var getTokenButton = $('.button');
    if (getTokenButton.length > 0) {
      getTokenButton.on('click', function(e) {
        e.preventDefault();
        
        // OAuth2 configuration
        var OURA_CONFIG = {
          CLIENT_ID: 'TGDTXUBGWULVNKSC',
          REDIRECT_URI: 'https://peppy-pothos-093b81.netlify.app/callback',
          AUTH_URL: 'https://cloud.ouraring.com/oauth/authorize'
        };
        
        // Build OAuth2 authorization URL
        var authUrl = OURA_CONFIG.AUTH_URL + 
          '?client_id=' + OURA_CONFIG.CLIENT_ID +
          '&redirect_uri=' + encodeURIComponent(OURA_CONFIG.REDIRECT_URI) +
          '&response_type=token' +
          '&scope=daily';
        
        // Open OAuth2 authorization in new window/tab
        window.open(authUrl, '_blank');
        
        // Show instructions
        alert('Authorization page opened! After authorizing:\n\n1. Copy the access token from the result page\n2. Return here and paste it in the Access Token field\n3. Click Save Settings');
      });
    }
  });
};

// Initialize Clay with custom function
var clay = new Clay(clayConfig, customClay, {
  autoHandleEvents: true
});

// Export for use in main app
module.exports = clay;
