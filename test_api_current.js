// Quick API test to check current Oura data
const https = require('https');

// Get today's date in YYYY-MM-DD format
function getLocalDateString() {
  const now = new Date();
  const year = now.getFullYear();
  const month = now.getMonth() + 1;
  const day = now.getDate();
  
  const monthStr = month < 10 ? '0' + month : month.toString();
  const dayStr = day < 10 ? '0' + day : day.toString();
  
  return year + '-' + monthStr + '-' + dayStr;
}

const today = getLocalDateString();
console.log('Testing Oura API for date:', today);

// Test readiness endpoint
const readinessUrl = `https://api.ouraring.com/v2/usercollection/daily_readiness?start_date=${today}&end_date=${today}`;

const options = {
  headers: {
    'Authorization': 'Bearer YOUR_TOKEN_HERE'
  }
};

console.log('Testing readiness endpoint...');
console.log('URL:', readinessUrl);

// Note: This is a template - you'll need to add your actual token
console.log('\nTo test manually, run:');
console.log(`curl -H "Authorization: Bearer YOUR_TOKEN" "${readinessUrl}"`);
