import AsyncStorage from '@react-native-async-storage/async-storage';

export const storeItem = async (key, value) => {
    try {
        console.log('storing', key, value);
        await AsyncStorage.setItem(key, value);
    } catch (e) {
    }
};

export const getItem = async (key) => {
    try {
        const value = await AsyncStorage.getItem(key);
        if (value !== null) {
            return value;
        }
    } catch (e) {
    }


};

export const removeItem = async (key) => {
    try {
        await AsyncStorage.removeItem(key);
    } catch (error) {

    }
}
