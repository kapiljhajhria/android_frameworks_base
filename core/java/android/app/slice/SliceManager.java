/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.app.slice;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemService;
import android.content.ContentResolver;
import android.content.Context;
import android.content.IContentProvider;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.ServiceManager.ServiceNotFoundException;
import android.util.ArrayMap;
import android.util.Pair;

import com.android.internal.util.Preconditions;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * Class to handle interactions with {@link Slice}s.
 * <p>
 * The SliceManager manages permissions and pinned state for slices.
 */
@SystemService(Context.SLICE_SERVICE)
public class SliceManager {

    private final ISliceManager mService;
    private final Context mContext;
    private final ArrayMap<Pair<Uri, SliceCallback>, ISliceListener> mListenerLookup =
            new ArrayMap<>();

    /**
     * @hide
     */
    public SliceManager(Context context, Handler handler) throws ServiceNotFoundException {
        mContext = context;
        mService = ISliceManager.Stub.asInterface(
                ServiceManager.getServiceOrThrow(Context.SLICE_SERVICE));
    }

    /**
     * Adds a callback to a specific slice uri.
     * <p>
     * This is a convenience that performs a few slice actions at once. It will put
     * the slice in a pinned state since there is a callback attached. It will also
     * listen for content changes, when a content change observes, the android system
     * will bind the new slice and provide it to all registered {@link SliceCallback}s.
     *
     * @param uri The uri of the slice being listened to.
     * @param callback The listener that should receive the callbacks.
     * @param specs The list of supported {@link SliceSpec}s of the callback.
     * @see SliceProvider#onSlicePinned(Uri)
     */
    public void registerSliceCallback(@NonNull Uri uri, @NonNull SliceCallback callback,
            @NonNull List<SliceSpec> specs) {
        registerSliceCallback(uri, callback, specs, Handler.getMain());
    }

    /**
     * Adds a callback to a specific slice uri.
     * <p>
     * This is a convenience that performs a few slice actions at once. It will put
     * the slice in a pinned state since there is a callback attached. It will also
     * listen for content changes, when a content change observes, the android system
     * will bind the new slice and provide it to all registered {@link SliceCallback}s.
     *
     * @param uri The uri of the slice being listened to.
     * @param callback The listener that should receive the callbacks.
     * @param specs The list of supported {@link SliceSpec}s of the callback.
     * @see SliceProvider#onSlicePinned(Uri)
     */
    public void registerSliceCallback(@NonNull Uri uri, @NonNull SliceCallback callback,
            @NonNull List<SliceSpec> specs, Handler handler) {
        try {
            mService.addSliceListener(uri, mContext.getPackageName(),
                    getListener(uri, callback, new ISliceListener.Stub() {
                        @Override
                        public void onSliceUpdated(Slice s) throws RemoteException {
                            handler.post(() -> callback.onSliceUpdated(s));
                        }
                    }), specs.toArray(new SliceSpec[specs.size()]));
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Adds a callback to a specific slice uri.
     * <p>
     * This is a convenience that performs a few slice actions at once. It will put
     * the slice in a pinned state since there is a callback attached. It will also
     * listen for content changes, when a content change observes, the android system
     * will bind the new slice and provide it to all registered {@link SliceCallback}s.
     *
     * @param uri The uri of the slice being listened to.
     * @param callback The listener that should receive the callbacks.
     * @param specs The list of supported {@link SliceSpec}s of the callback.
     * @see SliceProvider#onSlicePinned(Uri)
     */
    public void registerSliceCallback(@NonNull Uri uri, @NonNull SliceCallback callback,
            @NonNull List<SliceSpec> specs, Executor executor) {
        try {
            mService.addSliceListener(uri, mContext.getPackageName(),
                    getListener(uri, callback, new ISliceListener.Stub() {
                        @Override
                        public void onSliceUpdated(Slice s) throws RemoteException {
                            executor.execute(() -> callback.onSliceUpdated(s));
                        }
                    }), specs.toArray(new SliceSpec[specs.size()]));
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    private ISliceListener getListener(Uri uri, SliceCallback callback,
            ISliceListener listener) {
        Pair<Uri, SliceCallback> key = new Pair<>(uri, callback);
        if (mListenerLookup.containsKey(key)) {
            try {
                mService.removeSliceListener(uri, mContext.getPackageName(),
                        mListenerLookup.get(key));
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
            }
        }
        mListenerLookup.put(key, listener);
        return listener;
    }

    /**
     * Removes a callback for a specific slice uri.
     * <p>
     * Removes the app from the pinned state (if there are no other apps/callbacks pinning it)
     * in addition to removing the callback.
     *
     * @param uri The uri of the slice being listened to
     * @param callback The listener that should no longer receive callbacks.
     * @see #registerSliceCallback
     */
    public void unregisterSliceCallback(@NonNull Uri uri, @NonNull SliceCallback callback) {
        try {
            mService.removeSliceListener(uri, mContext.getPackageName(),
                    mListenerLookup.remove(new Pair<>(uri, callback)));
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Ensures that a slice is in a pinned state.
     * <p>
     * Pinned state is not persisted across reboots, so apps are expected to re-pin any slices
     * they still care about after a reboot.
     *
     * @param uri The uri of the slice being pinned.
     * @param specs The list of supported {@link SliceSpec}s of the callback.
     * @see SliceProvider#onSlicePinned(Uri)
     */
    public void pinSlice(@NonNull Uri uri, @NonNull List<SliceSpec> specs) {
        try {
            mService.pinSlice(mContext.getPackageName(), uri,
                    specs.toArray(new SliceSpec[specs.size()]));
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Remove a pin for a slice.
     * <p>
     * If the slice has no other pins/callbacks then the slice will be unpinned.
     *
     * @param uri The uri of the slice being unpinned.
     * @see #pinSlice
     * @see SliceProvider#onSliceUnpinned(Uri)
     */
    public void unpinSlice(@NonNull Uri uri) {
        try {
            mService.unpinSlice(mContext.getPackageName(), uri);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * @hide
     */
    public boolean hasSliceAccess() {
        try {
            return mService.hasSliceAccess(mContext.getPackageName());
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Get the current set of specs for a pinned slice.
     * <p>
     * This is the set of specs supported for a specific pinned slice. It will take
     * into account all clients and returns only specs supported by all.
     * @see SliceSpec
     */
    public @NonNull List<SliceSpec> getPinnedSpecs(Uri uri) {
        try {
            return Arrays.asList(mService.getPinnedSpecs(uri, mContext.getPackageName()));
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Turns a slice Uri into slice content.
     *
     * @param uri The URI to a slice provider
     * @param supportedSpecs List of supported specs.
     * @return The Slice provided by the app or null if none is given.
     * @see Slice
     */
    public @Nullable Slice bindSlice(@NonNull Uri uri, @NonNull List<SliceSpec> supportedSpecs) {
        Preconditions.checkNotNull(uri, "uri");
        ContentResolver resolver = mContext.getContentResolver();
        IContentProvider provider = resolver.acquireProvider(uri);
        if (provider == null) {
            throw new IllegalArgumentException("Unknown URI " + uri);
        }
        try {
            Bundle extras = new Bundle();
            extras.putParcelable(SliceProvider.EXTRA_BIND_URI, uri);
            extras.putParcelableArrayList(SliceProvider.EXTRA_SUPPORTED_SPECS,
                    new ArrayList<>(supportedSpecs));
            final Bundle res = provider.call(resolver.getPackageName(), SliceProvider.METHOD_SLICE,
                    null, extras);
            Bundle.setDefusable(res, true);
            if (res == null) {
                return null;
            }
            return res.getParcelable(SliceProvider.EXTRA_SLICE);
        } catch (RemoteException e) {
            // Arbitrary and not worth documenting, as Activity
            // Manager will kill this process shortly anyway.
            return null;
        } finally {
            resolver.releaseProvider(provider);
        }
    }

    /**
     * Turns a slice intent into slice content. Expects an explicit intent. If there is no
     * {@link android.content.ContentProvider} associated with the given intent this will throw
     * {@link IllegalArgumentException}.
     *
     * @param intent The intent associated with a slice.
     * @param supportedSpecs List of supported specs.
     * @return The Slice provided by the app or null if none is given.
     * @see Slice
     * @see SliceProvider#onMapIntentToUri(Intent)
     * @see Intent
     */
    public @Nullable Slice bindSlice(@NonNull Intent intent,
            @NonNull List<SliceSpec> supportedSpecs) {
        Preconditions.checkNotNull(intent, "intent");
        Preconditions.checkArgument(intent.getComponent() != null || intent.getPackage() != null,
                "Slice intent must be explicit " + intent);
        ContentResolver resolver = mContext.getContentResolver();

        // Check if the intent has data for the slice uri on it and use that
        final Uri intentData = intent.getData();
        if (intentData != null && SliceProvider.SLICE_TYPE.equals(resolver.getType(intentData))) {
            return bindSlice(intentData, supportedSpecs);
        }
        // Otherwise ask the app
        List<ResolveInfo> providers =
                mContext.getPackageManager().queryIntentContentProviders(intent, 0);
        if (providers == null) {
            throw new IllegalArgumentException("Unable to resolve intent " + intent);
        }
        String authority = providers.get(0).providerInfo.authority;
        Uri uri = new Uri.Builder().scheme(ContentResolver.SCHEME_CONTENT)
                .authority(authority).build();
        IContentProvider provider = resolver.acquireProvider(uri);
        if (provider == null) {
            throw new IllegalArgumentException("Unknown URI " + uri);
        }
        try {
            Bundle extras = new Bundle();
            extras.putParcelable(SliceProvider.EXTRA_INTENT, intent);
            extras.putParcelableArrayList(SliceProvider.EXTRA_SUPPORTED_SPECS,
                    new ArrayList<>(supportedSpecs));
            final Bundle res = provider.call(resolver.getPackageName(),
                    SliceProvider.METHOD_MAP_INTENT, null, extras);
            if (res == null) {
                return null;
            }
            return res.getParcelable(SliceProvider.EXTRA_SLICE);
        } catch (RemoteException e) {
            // Arbitrary and not worth documenting, as Activity
            // Manager will kill this process shortly anyway.
            return null;
        } finally {
            resolver.releaseProvider(provider);
        }
    }

    /**
     * Class that listens to changes in {@link Slice}s.
     */
    public interface SliceCallback {

        /**
         * Called when slice is updated.
         *
         * @param s The updated slice.
         * @see #registerSliceCallback
         */
        void onSliceUpdated(Slice s);
    }
}
