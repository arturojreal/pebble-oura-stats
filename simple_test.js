const https = require('https');

const TOKEN = process.argv[2] || 'YOUR_TOKEN_HERE';
const PROXY_URL = 'https://peppy-pothos-093b81.netlify.app/.netlify/functions/oura-proxy';

function testAPI(endpoint, name) {
  return new Promise((resolve) => {
    const today = new Date().toISOString().split('T')[0];
    const url = `${PROXY_URL}?endpoint=${encodeURIComponent(endpoint)}&token=${encodeURIComponent(TOKEN)}&start_date=${today}&end_date=${today}`;
    
    console.log(`\n🔄 Testing ${name} API for ${today}`);
    
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
            
            if (jsonData.data && jsonData.data.length > 0) {
              const latest = jsonData.data[jsonData.data.length - 1];
              console.log(`✅ Records found: ${jsonData.data.length}`);
              console.log(`📅 Latest date: ${latest.day || 'N/A'}`);
              
              if (endpoint === 'daily_readiness') {
                console.log(`🎯 Readiness Score: ${latest.score || 'N/A'}`);
                console.log(`🌡️  Temperature Deviation: ${latest.temperature_deviation || 'N/A'}`);
                console.log(`📈 Recovery Index: ${latest.recovery_index || 'N/A'}`);
              } else if (endpoint === 'daily_sleep') {
                console.log(`😴 Sleep Score: ${latest.score || 'N/A'}`);
                console.log(`⏰ Total Sleep Duration: ${latest.total_sleep_duration || 'N/A'} seconds`);
                console.log(`📊 Sleep Efficiency: ${latest.efficiency || 'N/A'}%`);
              }
              
              console.log(`🔍 All keys in latest record:`, Object.keys(latest));
            } else {
              console.log(`⚠️  No data records found for ${today}`);
            }
          } catch (e) {
            console.error(`❌ JSON Parse Error:`, e.message);
            console.log(`Raw response:`, data.substring(0, 300));
          }
        } else {
          console.error(`❌ HTTP Error: ${res.statusCode}`);
          console.log(`Response:`, data.substring(0, 300));
        }
        
        resolve();
      });
    }).on('error', (err) => {
      console.error(`❌ Network Error:`, err.message);
      resolve();
    });
  });
}

async function runTests() {
  console.log('🧪 Oura API Test - Checking Today\'s Data');
  console.log('==========================================');
  
  if (TOKEN === 'YOUR_TOKEN_HERE') {
    console.error('❌ Please provide your access token as an argument!');
    console.log('Usage: node simple_test.js YOUR_ACCESS_TOKEN');
    return;
  }
  
  await testAPI('daily_readiness', 'Readiness');
  await testAPI('daily_sleep', 'Sleep');
  
  console.log('\n✨ Testing complete!');
}

runTests();
