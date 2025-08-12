#!/usr/bin/env node

// Quick test script to check Oura API responses for readiness and sleep scores
const https = require('https');

// Configuration
const PROXY_URL = 'https://peppy-pothos-093b81.netlify.app/.netlify/functions/oura-proxy';
const TOKEN = 'YOUR_TOKEN_HERE'; // We'll get this from localStorage or manual input

function makeRequest(endpoint, callback) {
  const today = new Date().toISOString().split('T')[0];
  const url = `${PROXY_URL}?endpoint=${encodeURIComponent(endpoint)}&token=${encodeURIComponent(TOKEN)}&start_date=${today}&end_date=${today}`;
  
  console.log(`\n🔄 Testing ${endpoint} for date: ${today}`);
  console.log(`📡 URL: ${url.replace(TOKEN, TOKEN.substring(0, 10) + '...')}`);
  
  https.get(url, (res) => {
    let data = '';
    
    res.on('data', (chunk) => {
      data += chunk;
    });
    
    res.on('end', () => {
      console.log(`📊 Status: ${res.statusCode}`);
      
      if (res.statusCode === 200) {
        try {
          const jsonData = JSON.parse(data);
          console.log(`✅ Response received:`);
          
          if (jsonData.data && jsonData.data.length > 0) {
            const latest = jsonData.data[jsonData.data.length - 1];
            console.log(`   Records found: ${jsonData.data.length}`);
            console.log(`   Latest record keys:`, Object.keys(latest));
            
            if (endpoint === 'daily_readiness') {
              console.log(`   🎯 Readiness Score: ${latest.score || 'N/A'}`);
              console.log(`   🌡️  Temperature Deviation: ${latest.temperature_deviation || 'N/A'}`);
              console.log(`   📈 Recovery Index: ${latest.recovery_index || 'N/A'}`);
            } else if (endpoint === 'daily_sleep') {
              console.log(`   😴 Sleep Score: ${latest.score || 'N/A'}`);
              console.log(`   ⏰ Total Sleep Duration: ${latest.total_sleep_duration || 'N/A'}`);
              console.log(`   📊 Sleep Efficiency: ${latest.efficiency || 'N/A'}`);
            }
            
            console.log(`   📅 Date: ${latest.day || 'N/A'}`);
          } else {
            console.log(`   ⚠️  No data records found`);
          }
        } catch (e) {
          console.error(`❌ JSON Parse Error:`, e.message);
          console.log(`   Raw response (first 200 chars):`, data.substring(0, 200));
        }
      } else {
        console.error(`❌ HTTP Error: ${res.statusCode}`);
        console.log(`   Response:`, data.substring(0, 200));
      }
      
      callback();
    });
  }).on('error', (err) => {
    console.error(`❌ Network Error:`, err.message);
    callback();
  });
}

function testAPIs() {
  console.log('🧪 Oura API Test Script');
  console.log('========================');
  
  if (TOKEN === 'YOUR_TOKEN_HERE') {
    console.error('❌ Please set your Oura access token in the script first!');
    console.log('');
    console.log('💡 How to find your access token:');
    console.log('   1. Open your browser where you configured the Oura watchface');
    console.log('   2. Press F12 to open Developer Tools');
    console.log('   3. Go to Application tab > Local Storage');
    console.log('   4. Look for key "oura_access_token"');
    console.log('   5. Copy the value (it should start with something like "ABCD...")');
    console.log('');
    console.log('📝 Then run: node test_api.js YOUR_ACCESS_TOKEN_HERE');
    console.log('');
    console.log('⚠️  Note: The client ID (TGDTXUBGWULVNKSC) is different from the access token!');
    return;
  }
  
  // Test readiness API
  makeRequest('daily_readiness', () => {
    // Test sleep API
    makeRequest('daily_sleep', () => {
      console.log('\n✨ API testing complete!');
    });
  });
}

// Check if token is provided as command line argument
if (process.argv[2]) {
  // Replace the token constant
  const actualToken = process.argv[2];
  // Override the TOKEN constant
  eval(`const TOKEN = '${actualToken}'; testAPIs();`);
} else {
  testAPIs();
}
