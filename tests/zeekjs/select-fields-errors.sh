# @TEST-DOC: Try calling zeek.select_fields with an object that's not a valwrap.
# @TEST-EXEC: zeek select-fields.js
# @TEST-EXEC: btest-diff .stdout

@TEST-START-FILE select-fields.js
zeek.on('zeek_init', function() {
  let rec = {
    ts: 1234,
  };

  console.log('Wrong number of arguments');
  try {
    zeek.select_fields();
  } catch (error) {
    console.log(`Caught it: ${error}`);
  }

  console.log('Wrong type for record');
  try {
    zeek.select_fields("abc", zeek.ATTR_LOG);
  } catch (error) {
    console.log(`Caught it: ${error}`);
  }

  console.log('Plain object');
  try {
    zeek.select_fields(rec, zeek.ATTR_LOG);
  } catch (error) {
    console.log(`Caught it: ${error}`);
  }
});
@TEST-END-FILE
