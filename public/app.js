var dataFloatPath = 'test/float';
var dataIntPath = 'test/int';

const databaseFloat = database.ref(dataFloatPath);
const databaseInt = database.ref(dataIntPath);


var floatReading;
var intReading;

databaseFloat.on('value', (snapshot) => {
    floatReading = snapshot.val();
    console.log(floatReading);
    document.getElementById("reading-float").innerHTML = floatReading;
  }, (errorObject) => {
    console.log('The read failed: ' + errorObject.name);
  })

databaseInt.on('value', (snapshot) => {
  intReading = snapshot.val();
  console.log(intReading);
  document.getElementById("reading-int").innerHTML = intReading;
}, (errorObject) => {
  console.log('The read failed: ' + errorObject.name);
});