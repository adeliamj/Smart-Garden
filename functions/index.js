// functions/index.js

const functions = require('firebase-functions');
const admin = require('firebase-admin');
admin.initializeApp();

exports.scheduleWatering = functions.https.onCall(async (data, context) => {
    // Data yang diterima dari panggilan HTTP atau dari aplikasi klien
    const { date, hour, minute, duration } = data;

    try {
        // Lakukan validasi atau proses logika lainnya di sini sesuai kebutuhan aplikasi
        console.log(`Scheduling watering on ${date} at ${hour}:${minute} for ${duration} minutes.`);

        // Simpan jadwal penyiraman ke Firestore (contoh penggunaan Firestore)
        const scheduleRef = await admin.firestore().collection('wateringSchedules').add({
            date: date,
            time: `${hour}:${minute}`,
            duration: duration
        });

        console.log(`Watering scheduled successfully with ID: ${scheduleRef.id}`);

        return { success: true, scheduleId: scheduleRef.id };
    } catch (error) {
        console.error('Error scheduling watering:', error);
        throw new functions.https.HttpsError('internal', 'Error scheduling watering', error.message);
    }
});
